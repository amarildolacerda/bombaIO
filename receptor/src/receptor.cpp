#include "receptor.h"
#include "Arduino.h"
#include <LoRaRF95.h>
#include "logger.h"
#include "config.h"

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define COMSerial Serial1
#define ShowSerial SerialUSB
#endif

#ifdef ARDUINO_ARCH_STM32F4
#define COMSerial Serial
#define ShowSerial SerialUSB
#endif

const int LED_PIN = LED_BUILTIN;
const int RELAY_PIN = 2;
const long STATUS_INTERVAL = 5000;
const long PRESENTATION_INTERVAL = 10000;
unsigned long previousMillis = 0;
volatile bool pinStateChanged = false;
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

    if (!lora.initialize(Config::BAND, Config::TERMINAL_ID, Config::PROMISCUOS))
    {
        Logger::log(LogLevel::ERROR, "LoRa initialization failed!");
    }

    digitalWrite(LED_PIN, LOW);
}

void formatMessage(uint8_t tid, const char *event, const char *value = "")
{
    sprintf(messageBuffer,
            "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}",
            Config::TERMINAL_NAME, event, value);
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
        sendFormattedMessage(0, "presentation", ("RSSI:" + String(lora.getLastRssi())).c_str());
        // Logger::info("Presentation");
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
    bool handled = false;
    if (message == nullptr)
    {
        // ack(false, tid);
        return false;
    }

    // Logger::debug(message);

    constexpr const char *keywordsNoAck[] PROGMEM = {"ack", "nak"};
    constexpr const char *keywordsAck[] PROGMEM = {"presentation", "get", "set", "gpio"};

    if ((strstr_P(message, PSTR("get")) != nullptr) && (strstr_P(message, PSTR("status")) != nullptr))
    {
        pinStateChanged = true;
        handled = true;
    }
    else if (strstr_P(message, PSTR("\"off\"")) != nullptr)
    {
        digitalWrite(RELAY_PIN, LOW);
        handled = Logger::log(LogLevel::INFO, "Relay OFF");
    }
    else if (strstr_P(message, PSTR("\"on\"")) != nullptr)
    {
        digitalWrite(RELAY_PIN, HIGH);
        handled = Logger::log(LogLevel::INFO, "Relay ON");
    }
    else if (strstr_P(message, PSTR("toggle")) != nullptr)
    {
        int currentState = digitalRead(RELAY_PIN);
        digitalWrite(RELAY_PIN, !currentState);
        handled = Logger::log(LogLevel::INFO, "Relay toggled ");
    }
    else if (strstr_P(message, PSTR("presentation")) != nullptr)
    {
        mustPresentation = true;
        handled = true;
    }
    else if (strstr_P(message, PSTR("ping")) != nullptr)
    {
        lora.sendMessage(tid, "pong");
        // nao responde com ack nem nak
        return true;
    }
    else
    {
        // Verifica se message contém algum dos valores no vetor (nao responde)
        for (const char *keyword : keywordsNoAck)
        {
            if (strstr_P(message, keyword) != nullptr)
            {
                // nao responde ACK nem NAK
                return true;
            }
        }
        for (const char *keyword : keywordsAck)
        {
            if (strstr_P(message, keyword) != nullptr)
            {
                handled = true;
            }
        }
    }
    ack(handled, tid);
    return handled;
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
        processAndRespondToMessage((const char *)loraBuffer);
    }
}

void loop()
{
    handleLoraIncomingMessages();
    if ((pinStateChanged) || (millis() - previousMillis >= STATUS_INTERVAL))
    {
        previousMillis = millis();
        sendStatus();
        pinStateChanged = false;
        int rssi = lora.getLastRssi();
        if (rssi != 0)
        {
            Logger::log(LogLevel::WARNING, String("RSSI: " + String(rssi) + " dBm").c_str());
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