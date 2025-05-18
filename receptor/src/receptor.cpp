#include "receptor.h"
#include "Arduino.h"
#include "config.h"
#ifndef x__AVR__
#include "ArduinoJson.h"
#endif
#ifdef LORA
#include <LoRaRF95.h>
#endif
#include "logger.h"
#include "html_server.h"

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define COMSerial Serial1
#define ShowSerial SerialUSB
#endif

#ifdef ARDUINO_ARCH_STM32F4
#define COMSerial Serial
#define ShowSerial SerialUSB
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#endif

#ifdef LORA
static char messageBuffer[Config::MESSAGE_MAX_LEN];
static uint8_t loraBuffer[Config::MESSAGE_MAX_LEN];
#endif

#ifdef __AVR__
#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define IDENTIFICATION_TIMEOUT_MS 10000UL      // 10 segundos para zerar contagem
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL // 1 minuto para 3 boots
#endif

void sendStatus();
void sendPresentation(uint8_t n = 3);
void ack(bool ak = true, uint8_t targetTerminal = 0x00);
void formatMessage(uint8_t tid, const char *event, const char *value);
void sendFormattedMessage(uint8_t tid, const char *event, const char *value);
bool processAndRespondToMessage(const char *message);
void handleLoraIncomingMessages();

bool loraActive = false;
#ifdef __AVR__
void checkAndHandleIdentificationMode();
bool waitAck(const uint32_t timeout);
#endif

#ifdef __AVR__

bool waitAck(const uint32_t timeout)
{
    char msg[Config::MESSAGE_MAX_LEN];
    uint8_t len = sizeof(msg);
    const long start = millis();
    while (millis() - start < timeout)
    {
        if (lora.receiveMessage(msg, len))
        {
            if (String(msg).indexOf("ack") >= 0)
                return true;
        }
        delay(10);
        yield();
    }

    return false;
}

void zeraContadorReinicio()
{
    uint8_t bootCount = 0;
    EEPROM.write(EEPROM_ADDR_BOOT_COUNT, bootCount);
    Logger::log(LogLevel::INFO, F("Zerou contador de reinicio"));
}
void checkAndHandleIdentificationMode()
{
    uint8_t bootCount = EEPROM.read(EEPROM_ADDR_BOOT_COUNT);
    bootCount++;
    Serial.print("Boot Count: ");
    Serial.println(bootCount);

    // Salva novo estado
    EEPROM.write(EEPROM_ADDR_BOOT_COUNT, bootCount);

    // Verificação de gravação
    uint8_t checkBootCount = EEPROM.read(EEPROM_ADDR_BOOT_COUNT);
    if (checkBootCount != bootCount)
    {
        Logger::log(LogLevel::ERROR, F("Falha ao gravar na EEPROM!"));
        Serial.print(bootCount);
    }
    else
    {
        Logger::log(LogLevel::DEBUG, F("EEPROM gravada com sucesso."));
    }

    if (bootCount >= 3)
    {
        Logger::log(LogLevel::WARNING, F("Entrando em modo de identificação do dispositivo!"));
        // Modo identificação: envia apresentação continuamente
        for (int i = 0; i < 10; ++i)
        {
            Serial.print("presentation");
            sendPresentation(1);
            delay(1000);
            if (waitAck(200))
            {
                break;
            }
        }
        zeraContadorReinicio();
    }
}
#endif

// Define systemState struct and global instance

void setup()
{
    Logger::setLogLevel(LogLevel::VERBOSE);
#ifdef __AVR__
    ShowSerial.begin(115200);
    COMSerial.begin(115200);
    while (!ShowSerial)
        ;
    if (!lora.initialize(Config::BAND, Config::TERMINAL_ID, Config::PROMISCUOS))
    {
        Logger::log(LogLevel::ERROR, F("LoRa initialization failed!"));
        loraActive = false;
    }
    else
    {
        loraActive = true;
    }
    if (loraActive)
        checkAndHandleIdentificationMode();
#elif ESP8266
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");

    delay(5000);

    // Serial.begin(9600);

#endif
    Logger::log(LogLevel::INFO, F("RF95 Server Initializing..."));

    pinMode(Config::LED_PIN, OUTPUT);
    pinMode(Config::RELAY_PIN, OUTPUT);
    digitalWrite(Config::LED_PIN, HIGH);

    systemState.lastPinState = digitalRead(Config::RELAY_PIN);

#ifdef ESP8266
    initHtmlServer();
#endif

    digitalWrite(Config::LED_PIN, LOW);
    systemState.pinStateChanged = true;
}

void formatMessage(uint8_t tid, const char *event, const char *value = "")
{
#ifdef LORA
    memset(messageBuffer, 0, sizeof(messageBuffer));
    snprintf(messageBuffer, sizeof(messageBuffer),
             "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}",
             Config::TERMINAL_NAME, event, value);
#endif
}

void sendFormattedMessage(uint8_t tid, const char *event, const char *value)
{
#ifdef LORA
    formatMessage(tid, event, value);
    lora.sendMessage(tid, messageBuffer);
#endif
}

void sendPresentation(uint8_t n)
{
#ifdef LORA
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {
        String attemptMessage = String(attempt + 1);
        sendFormattedMessage(0, "presentation", ("RSSI:" + String(lora.getLastRssi())).c_str());
        // Logger::info("Presentation");
    }
#endif
}

void sendStatus()
{
    int currentState = digitalRead(Config::RELAY_PIN);
    const char *status = currentState == HIGH ? "on" : "off";
    for (uint8_t i = 0; i < 3; i++)
    {
        sendFormattedMessage(0x00, "status", status);
        systemState.lastPinState = currentState;
        systemState.pinStateChanged = true;
        delay(1000);
        if (waitAck(1000))
        {
            systemState.pinStateChanged = false;
            Logger::log(LogLevel::INFO, F("status ack OK"));
            break;
        }
    }
}

void setTime(const char *tz)
{
    Serial.print("TimeZ: ");
    Serial.println(tz);
}

bool processAndRespondToMessage(const char *message)
{
#ifdef LORA
    uint8_t tid = 0; // Placeholder for terminal ID
    bool handled = false;
    if (message == nullptr)
    {
        return false;
    }
    digitalWrite(Config::LED_PIN, HIGH);

    const char *keywordsNoAck[] = {"ack", "nak"};
    const char *keywordsAck[] = {"presentation", "get", "set", "gpio"};

    if ((strstr_P(message, PSTR("get")) != nullptr) && (strstr_P(message, PSTR("status")) != nullptr))
    {
        systemState.pinStateChanged = true;
        handled = true;
    }
    else if (strstr_P(message, PSTR("\"off\"")) != nullptr)
    {
        digitalWrite(Config::RELAY_PIN, LOW);
        systemState.pinStateChanged = true;
        handled = Logger::log(LogLevel::INFO, F("Relay OFF"));
    }
    else if (strstr_P(message, PSTR("\"on\"")) != nullptr)
    {
        digitalWrite(Config::RELAY_PIN, HIGH);
        systemState.pinStateChanged = true;
        handled = Logger::log(LogLevel::INFO, F("Relay ON"));
    }
    else if (strstr_P(message, PSTR("toggle")) != nullptr)
    {
        int currentState = digitalRead(Config::RELAY_PIN);
        digitalWrite(Config::RELAY_PIN, !currentState);
        systemState.pinStateChanged = true;
        handled = Logger::log(LogLevel::INFO, F("Relay toggled "));
    }
    else if (strstr_P(message, PSTR("presentation")) != nullptr)
    {
        systemState.mustPresentation = true;
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
#ifndef x__AVR__
        JsonDocument doc;

        // Deserializa a string JSON
        DeserializationError error = deserializeJson(doc, message);

        // Verifica se ocorreu um erro
        if (error)
        {
            Logger::log(LogLevel::ERROR, F("Falha na deserialização"));
            ack(false, tid);
            return false;
        }

        // Extrai o valor da chave "value"
        const char *value = doc["value"];
        const char *event = doc["event"];
        if (strstr("time", event) != nullptr)
        {
            setTime(value);
            handled = true;
        }
#endif
    }

    if (!handled)
    {
        for (const char *keyword : keywordsNoAck)
        {
            if (strstr_P(message, keyword) != nullptr)
            {
                // nao responde ACK nem NAK
                Logger::log(LogLevel::VERBOSE, keyword);
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
#else
    return false;
#endif
}

void ack(bool ak, uint8_t tid)
{
#ifdef LORA
    // Garante que a mensagem enviada está null-terminated
    char ackMsg[8];
    memset(ackMsg, 0, sizeof(ackMsg));
    if (ak)
    {
        strncpy(ackMsg, "ack", sizeof(ackMsg));
    }
    else
    {
        strncpy(ackMsg, "nak", sizeof(ackMsg));
    }
    lora.sendMessage(tid, ackMsg);
#endif
}

void handleLoraIncomingMessages()
{
#ifdef LORA
    if (lora.available())
    {
        uint8_t len = RH_RF95_MAX_MESSAGE_LEN;
        if (lora.receiveMessage((char *)loraBuffer, len))
        {
            processAndRespondToMessage((const char *)loraBuffer);
        }
    }
#endif
}

long checaZeraContador = millis();
bool zerou = false;
void loop()
{

    if ((!loraActive))
    {
        digitalWrite(Config::LED_PIN, HIGH);
        delay(1000);
        digitalWrite(Config::LED_PIN, LOW);
        delay(500);
        return;
    }

    if ((!zerou) && (millis() - checaZeraContador > 60000))
    { // depois de um minuto rodando, reset o contador de reinicio;
        zeraContadorReinicio();
        zerou = true;
    }

#ifdef LORA
    handleLoraIncomingMessages();
    unsigned long status_internal = (systemState.pinStateChanged ? 5000 : Config::STATUS_INTERVAL);
    if ((millis() - systemState.previousMillis) >= status_internal)
    {
        systemState.previousMillis = millis();
        sendStatus();
        systemState.pinStateChanged = false;
        int rssi = lora.getLastRssi();
        if (rssi != 0)
        {
            Logger::log(LogLevel::WARNING, String("RSSI: " + String(rssi) + " dBm").c_str());
        }
    }

#endif

#ifdef ESP8266
    processHtmlServer();
#endif
    digitalWrite(Config::LED_PIN, LOW);
}