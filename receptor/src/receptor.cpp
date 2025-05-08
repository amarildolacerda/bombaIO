#include "receptor.h"
#include "Arduino.h"
#include <RH_RF95.h>
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial SSerial(10, 11); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial
#endif

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define COMSerial Serial1
#define ShowSerial SerialUSB
#endif

#ifdef ARDUINO_ARCH_STM32F4
#define COMSerial Serial
#define ShowSerial SerialUSB
#endif

const char dtype[] = "relay";
const int LED_PIN = LED_BUILTIN;
const int RELAY_PIN = 2;
const long STATUS_INTERVAL = 5000;
const long PRESENTATION_INTERVAL = 10000;
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
void handleLoraIncomingMessages();
#ifdef __AVR__
void printFreeMemory();
#endif

class LoRaRF95
{
public:
    LoRaRF95() : rf95(COMSerial) {}

    bool initialize(float frequency, uint8_t terminalId, bool promiscuous = true)

    {
        if (!rf95.init())
        {
            Logger::log(LogLevel::INFO, "LoRa initialization failed!");
            return false;
        }
        COMSerial.setTimeout(0);
        rf95.setFrequency(frequency);
        rf95.setPromiscuous(promiscuous);
        rf95.setHeaderTo(0xFF);
        rf95.setHeaderFrom(terminalId);
        rf95.setTxPower(14);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        Logger::log(LogLevel::INFO, "LoRa Server Ready");
        return true;
    }

    void sendMessage(uint8_t tid, const char *message)
    {
        rf95.setHeaderTo(tid);
        rf95.setHeaderId(genHeaderId());
        rf95.send((uint8_t *)message, strlen(message));
        if (!rf95.waitPacketSent())
        {
            Logger::log(LogLevel::ERROR, "Failed to send message");
        }
        else
        {
            Logger::log(LogLevel::DEBUG, message);
        }
    }

    bool receiveMessage(char *buffer, uint8_t &len)
    {
        if (rf95.available())
        {
            if (rf95.recv((uint8_t *)buffer, &len))
            {
                buffer[len] = '\0';
                return true;
            }
        }
        return false;
    }

    int getLastRssi()
    {
        return rf95.lastRssi();
    }

private:
    RH_RF95<decltype(COMSerial)> rf95;
    uint8_t nHeaderId = 0;

    uint8_t genHeaderId()
    {
        if (nHeaderId >= 255)
            nHeaderId = 0;
        return nHeaderId++;
    }
};

// Global instance of LoRaRF95
LoRaRF95 lora;

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

    if (!lora.initialize(868.0, TERMINAL_ID))
    {
        Logger::log(LogLevel::ERROR, "LoRa initialization failed!");
    }

    digitalWrite(LED_PIN, LOW);
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
    lora.sendMessage(tid, messageBuffer);
    digitalWrite(LED_PIN, LOW);
}

void sendPresentation(uint8_t n)
{
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {
        String attemptMessage = String(attempt + 1);
        sendFormattedMessage(0, "presentation", attemptMessage.c_str());
        Logger::info("Presentation");
    }
}

void sendStatus()
{
    int currentState = digitalRead(RELAY_PIN);
    const char *status = currentState == HIGH ? "on" : "off";
    sendFormattedMessage(0x00, "status", status);
    lastPinState = currentState;
    pinStateChanged = false;
}

bool processAndRespondToMessage(const char *message)
{
    uint8_t tid = 0; // Placeholder for terminal ID
    if (message == nullptr)
        return false;

    Logger::debug(message);
    if (strstr_P(message, PSTR("status")) != nullptr)
    {
        pinStateChanged = true;
        ack(true, tid);
    }
    else if (strstr_P(message, PSTR("ligar")) != nullptr)
    {
        ack(true, tid);
        digitalWrite(RELAY_PIN, LOW);
        Logger::log(LogLevel::INFO, "Relay ON");
    }
    else if (strstr_P(message, PSTR("desligar")) != nullptr)
    {
        ack(true, tid);
        digitalWrite(RELAY_PIN, HIGH);
        Logger::log(LogLevel::INFO, "Relay OFF");
    }
    else if (strstr_P(message, PSTR("reverter")) != nullptr)
    {
        ack(true, tid);
        int currentState = digitalRead(RELAY_PIN);
        digitalWrite(RELAY_PIN, !currentState);
        Logger::log(LogLevel::INFO, "Relay toggled ");
    }
    else if (strstr_P(message, PSTR("presentation")) != nullptr)
    {
        ack(true, tid);
        mustPresentation = true;
    }
    else if (strstr_P(message, PSTR("ping")) != nullptr)
    {
        lora.sendMessage(tid, "pong");
        Logger::log(LogLevel::INFO, "Pong sent");
    }
    else
    {
        return false;
    }
    return true;
}

void ack(bool ak, uint8_t tid)
{
    lora.sendMessage(tid, ak ? "ack" : "nak");
}

void handleLoraIncomingMessages()
{
    uint8_t len = RH_RF95_MAX_MESSAGE_LEN;
    if (lora.receiveMessage((char *)loraBuffer, len))
    {
        Logger::log(LogLevel::INFO, "Mensagem recebida: ");
        if (!processAndRespondToMessage((const char *)loraBuffer))
        {
            ack(false, 0x00);
        }
    }
}

void loop()
{
    if ((pinStateChanged) || (millis() - previousMillis >= STATUS_INTERVAL))
    {
        previousMillis = millis();
        sendStatus();
        pinStateChanged = false;
        int rssi = lora.getLastRssi();
        if (rssi != 0)
        {
            Logger::log(LogLevel::INFO, String("RSSI: " + String(rssi) + " dBm").c_str());
        }
    }

    handleLoraIncomingMessages();
    static unsigned long lastPresentationMillis = 0;
    if (mustPresentation || millis() - lastPresentationMillis >= PRESENTATION_INTERVAL)
    {
        sendPresentation(1);
        mustPresentation = false;
        lastPresentationMillis = millis();
    }
}