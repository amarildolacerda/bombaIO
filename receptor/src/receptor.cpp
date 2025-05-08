#include "receptor.h"
#include "Arduino.h"
#include <RH_RF95.h>
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial SSerial(10, 11); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial
RH_RF95<SoftwareSerial> rf95(COMSerial);
#endif

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define COMSerial Serial1
#define ShowSerial SerialUSB
RH_RF95<Uart> rf95(COMSerial);
#endif

#ifdef ARDUINO_ARCH_STM32F4
#define COMSerial Serial
#define ShowSerial SerialUSB
RH_RF95<HardwareSerial> rf95(COMSerial);
#endif

const char dtype[] = "relay";
const int LED_PIN = LED_BUILTIN;
const int RELAY_PIN = 2;
const long STATUS_INTERVAL = 5000;
unsigned long previousMillis = 0;
volatile bool pinStateChanged = false;
const int TERMINAL_ID = 0x01;
volatile int lastPinState = HIGH;
bool mustPresentation = true;

char messageBuffer[RH_RF95_MAX_MESSAGE_LEN];
uint8_t loraBuffer[RH_RF95_MAX_MESSAGE_LEN];

void sendStatus();
void sendPresentation(uint8_t n = 3);
void ack(bool ak = true, uint8_t targetTerminal = 0x00);
void formatMessage(uint8_t tid, const char *event, const char *value);
void sendFormattedMessage(uint8_t tid, const char *event, const char *value);
bool processAndRespondToMessage(const char *message);
bool waitAck();
void handleLoraIncomingMessages();
uint8_t genHeaderId();
#ifdef __AVR__
void printFreeMemory();
#endif

#if defined(ESP8266) || defined(ESP32)
void IRAM_ATTR handleInterrupt()
#else
void handleInterrupt()
#endif
{
    static unsigned long last_interrupt = 0;
    unsigned long now = millis();
    if (now - last_interrupt > 50)
    {
        pinStateChanged = true;
        digitalWrite(LED_PIN, HIGH);
    }
    last_interrupt = now;
}

#ifdef __AVR__
void printFreeMemory()
{
    // ShowSerial.print(F("Free RAM: "));
    // ShowSerial.print(freeMemory());
    // ShowSerial.println(F(" bytes"));
}
#endif

void setup()
{

    Logger::setLogLevel(LogLevel::VERBOSE);
    ShowSerial.begin(115200);
    COMSerial.begin(115200);

    while (!ShowSerial)
        ;
    Logger::log(LogLevel::INFO, "RF95 Server Initializing...");

    pinMode(LED_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    attachInterrupt(digitalPinToInterrupt(RELAY_PIN), handleInterrupt, CHANGE);

    lastPinState = digitalRead(RELAY_PIN);

    if (!rf95.init())
    {
        Logger::log(LogLevel::INFO, "LoRa initialization failed!");
    }

    // Configure LoRa parameters
    rf95.setFrequency(868.0);
    rf95.setPromiscuous(true);
    rf95.setHeaderTo(0xFF);
    rf95.setHeaderFrom(TERMINAL_ID);

    Logger::log(LogLevel::INFO, "LoRa Server Ready");
    digitalWrite(LED_PIN, LOW);
}

bool waitAck()
{
    unsigned long start = millis();
    while (millis() - start < 5000)
    {
        digitalWrite(LED_PIN, HIGH);
        YIELD

        if (rf95.available())
        {
            uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
            uint8_t len = sizeof(buf);

            if (rf95.recv(buf, &len))
            {
                buf[len] = '\0';
                Logger::log(LogLevel::INFO, "ACK recebido "); // + (char *)buf);
                digitalWrite(LED_PIN, LOW);
                return true;
            }
        }
        digitalWrite(LED_PIN, LOW);
        delay(50);
    }
    Logger::log(LogLevel::INFO, "ACK timeout");
    return false;
}

void formatMessage(uint8_t tid, const char *event, const char *value = "")
{
    sprintf(messageBuffer,
            "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}",
            dtype, event, value);
}

void sendFormattedMessage(uint8_t tid, const char *event, const char *value)
{
    digitalWrite(LED_PIN, HIGH);
    formatMessage(tid, event, value);
    rf95.setHeaderTo(tid);
    rf95.setHeaderId(genHeaderId());
    // rf95.send((uint8_t *)"teste", 5);
    rf95.send((uint8_t *)messageBuffer, strlen(messageBuffer));
    if (!rf95.waitPacketSent())
    {
        Logger::log(LogLevel::ERROR, "Não conseguiu enviar o packet"); // + messageBuffer);
    }
    else
        Logger::log(LogLevel::INFO, "Mensagem enviada"); // + messageBuffer);
    // Serial.println(messageBuffer);
    Logger::log(LogLevel::DEBUG, messageBuffer);
    digitalWrite(LED_PIN, LOW);

    delay(100);
}

void sendPresentation(uint8_t n)
{
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {
        String attemptMessage = String(attempt); // String("presentation attempt ") + (attempt + 1);
        // Logger::log(LogLevel::INFO, String("Enviando apresentação: tentativa ") + (attempt + 1));
        sendFormattedMessage(0, "presentation", attemptMessage.c_str());
        delay(1000); // Delay antes de esperar o ACK
        ackReceived = waitAck();
        if (!ackReceived)
        {
            YIELD
            /// Logger::log(LogLevel::WARNING, String("Tentativa ") + (attempt + 1) + " falhou");
            delay(100); // Delay adicional antes da próxima tentativa
        }
    }

    if (ackReceived)
    {
        mustPresentation = false;
        Logger::log(LogLevel::INFO, "Apresentação bem-sucedida");
    }
    else
    {
        Logger::log(LogLevel::ERROR, "Falha na apresentação"); // + String(n) + " tentativas");
    }
}

uint8_t nHeaderId = 0;
uint8_t genHeaderId()
{
    if (nHeaderId >= 255)
        nHeaderId = 0;
    return nHeaderId++;
}

void sendStatus()
{
    int currentState = digitalRead(RELAY_PIN);
    const char *status = currentState == HIGH ? "off" : "on";
    sendFormattedMessage(0x00, "status", status);
    lastPinState = currentState;
    pinStateChanged = false;
}

bool processAndRespondToMessage(const char *message)
{
    if (message == nullptr)
        return false;

    if (strstr_P(message, PSTR("status")) != nullptr)
    {
        sendStatus();
    }
    else if (strstr_P(message, PSTR("ligar")) != nullptr)
    {
        digitalWrite(RELAY_PIN, LOW);
        Logger::log(LogLevel::INFO, "Relay ON");
    }
    else if (strstr_P(message, PSTR("desligar")) != nullptr)
    {
        digitalWrite(RELAY_PIN, HIGH);
        Logger::log(LogLevel::INFO, "Relay OFF");
    }
    else if (strstr_P(message, PSTR("reverter")) != nullptr)
    {
        int currentState = digitalRead(RELAY_PIN);
        digitalWrite(RELAY_PIN, !currentState);
        Logger::log(LogLevel::INFO, "Relay toggled "); // + (!currentState) ? "ON" : "OFF");
    }
    else if (strstr_P(message, PSTR("presentation")) != nullptr)
    {
        ack(true, 0);
        mustPresentation = true;
    }
    else if (strstr_P(message, PSTR("ping")) != nullptr)
    {
        rf95.send((uint8_t *)"pong", 4);
        rf95.waitPacketSent();
        Logger::log(LogLevel::INFO, "Pong sent");
    }
    else
    {
        return false;
    }
    return true;
}

void ack(bool ak, uint8_t targetTerminal)
{
    const char *ackMessage = ak ? "ACK" : "NAK";
    sendFormattedMessage(targetTerminal, "ack", ackMessage);
}

void handleLoraIncomingMessages()
{
    if (rf95.available())
    {
        Logger::log(LogLevel::VERBOSE, "Incoming Messages");
        uint8_t len = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv(loraBuffer, &len))
        {
            if (len >= sizeof(loraBuffer))
                len = sizeof(loraBuffer) - 1;
            loraBuffer[len] = '\0';
            Logger::log(LogLevel::INFO, "Mensagem recebida: "); // + String((char *)loraBuffer));
            if (!processAndRespondToMessage((const char *)loraBuffer))
            {
                ack(false, 0x00);
            }
        }
    }
}

void loop()
{
    if (pinStateChanged)
    {
        pinStateChanged = false;
        int currentState = digitalRead(RELAY_PIN);
        if (currentState != lastPinState)
        {
            sendStatus();
        }
    }

    if (millis() - previousMillis >= STATUS_INTERVAL)
    {
        previousMillis = millis();
        sendStatus();
    }

    handleLoraIncomingMessages();

    static unsigned long lastPresentationMillis = 0;
    if (mustPresentation || millis() - lastPresentationMillis >= 60000)
    {
        sendPresentation(1);
        mustPresentation = false;
        lastPresentationMillis = millis();
    }
}