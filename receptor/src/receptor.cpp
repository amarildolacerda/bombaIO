#include "receptor.h"
#include "Arduino.h"
#include "config.h"

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
char messageBuffer[RH_RF95_MAX_MESSAGE_LEN];
uint8_t loraBuffer[RH_RF95_MAX_MESSAGE_LEN];
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
#ifdef __AVR__
void printFreeMemory();
void checkAndHandleIdentificationMode();
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
        systemState.pinStateChanged = true;
        digitalWrite(Config::LED_PIN, HIGH);
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

bool waitAck(const uint32_t timeout)
{
    char msg[64];
    uint8_t len = sizeof(msg);
    const long start = millis();
    if (millis() - start > timeout)
        Serial.print("waitAck");
    if (lora.receiveMessage(msg, len))
    {
        if (String(msg).indexOf("ack") >= 0)
            return true;
    }

    return false;
}

void checkAndHandleIdentificationMode()
{
    uint8_t bootCount = EEPROM.read(EEPROM_ADDR_BOOT_COUNT);
    unsigned long lastBoot = 0;
    for (int i = 0; i < 4; ++i)
    {
        lastBoot |= ((unsigned long)EEPROM.read(EEPROM_ADDR_LAST_BOOT + i)) << (8 * i);
    }
    Serial.print("lastBoot: ");
    Serial.print(lastBoot);
    unsigned long now = millis();
    // Remover ajuste de overflow, pois millis() sempre zera no boot
    // O correto é usar um contador simples de boots consecutivos
    if ((now - lastBoot > IDENTIFICATION_POWER_WINDOW_MS))
    {
        Serial.print("ReinicioBootBoot: ");

        bootCount = 1;
    }
    else
    {
        bootCount++;
    }

    Serial.print(" Count: ");
    Serial.println(bootCount);

    // Salva novo estado
    EEPROM.write(EEPROM_ADDR_BOOT_COUNT, bootCount);
    for (int i = 0; i < 4; ++i)
    {
        EEPROM.write(EEPROM_ADDR_LAST_BOOT + i, (now >> (8 * i)) & 0xFF);
    }
    // Verificação de gravação
    uint8_t checkBootCount = EEPROM.read(EEPROM_ADDR_BOOT_COUNT);
    unsigned long checkLastBoot = 0;
    for (int i = 0; i < 4; ++i)
    {
        checkLastBoot |= ((unsigned long)EEPROM.read(EEPROM_ADDR_LAST_BOOT + i)) << (8 * i);
    }
    if (checkBootCount != bootCount || checkLastBoot != now)
    {
        Logger::log(LogLevel::ERROR, "Falha ao gravar na EEPROM!");
        Serial.print(bootCount);
    }
    else
    {
        Logger::log(LogLevel::DEBUG, "EEPROM gravada com sucesso.");
    }

    if (bootCount >= 3)
    {
        Logger::log(LogLevel::WARNING, "Entrando em modo de identificação do dispositivo!");
        // Modo identificação: envia apresentação continuamente
        for (int i = 0; i < 10; ++i)
        {
            Serial.print("presetattion");
            sendPresentation(1);
            delay(1000);
            if (waitAck(200))
            {
                break;
            }
        }
        bootCount = 0;
        EEPROM.write(EEPROM_ADDR_BOOT_COUNT, bootCount);
    }
}
#endif

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
        Logger::log(LogLevel::ERROR, "LoRa initialization failed!");
    }
    checkAndHandleIdentificationMode();
#elif ESP8266
    Serial.begin(115200);
    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");

    delay(5000);

    // Serial.begin(9600);

#endif
    Logger::log(LogLevel::INFO, "RF95 Server Initializing...");

    pinMode(Config::LED_PIN, OUTPUT);
    pinMode(Config::RELAY_PIN, OUTPUT);
    digitalWrite(Config::RELAY_PIN, HIGH);
    digitalWrite(Config::LED_PIN, HIGH);

    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), handleInterrupt, CHANGE);

    systemState.lastPinState = digitalRead(Config::RELAY_PIN);

#ifdef ESP8266
    initHtmlServer();
#endif

    digitalWrite(Config::LED_PIN, LOW);
}

void formatMessage(uint8_t tid, const char *event, const char *value = "")
{
#ifdef LORA
    sprintf(messageBuffer,
            "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}",
            Config::TERMINAL_NAME, event, value);
#endif
}

void sendFormattedMessage(uint8_t tid, const char *event, const char *value)
{
#ifdef LORA
    digitalWrite(Config::LED_PIN, HIGH);
    formatMessage(tid, event, value);
    lora.sendMessage(tid, messageBuffer);
    digitalWrite(Config::LED_PIN, LOW);
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
    sendFormattedMessage(0x00, "status", status);
    systemState.lastPinState = currentState;
    systemState.pinStateChanged = false;
}

bool processAndRespondToMessage(const char *message)
{
#ifdef LORA
    uint8_t tid = 0; // Placeholder for terminal ID
    bool handled = false;
    if (message == nullptr)
    {
        // ack(false, tid);
        return false;
    }

    // Logger::debug(message);

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
        handled = Logger::log(LogLevel::INFO, "Relay OFF");
    }
    else if (strstr_P(message, PSTR("\"on\"")) != nullptr)
    {
        digitalWrite(Config::RELAY_PIN, HIGH);
        handled = Logger::log(LogLevel::INFO, "Relay ON");
    }
    else if (strstr_P(message, PSTR("toggle")) != nullptr)
    {
        int currentState = digitalRead(Config::RELAY_PIN);
        digitalWrite(Config::RELAY_PIN, !currentState);
        handled = Logger::log(LogLevel::INFO, "Relay toggled ");
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
#else
    return false;
#endif
}

void ack(bool ak, uint8_t tid)
{
#ifdef LORA
    lora.sendMessage(tid, ak ? "ack" : "nak");
#endif
}

void handleLoraIncomingMessages()
{
#ifdef LORA
    uint8_t len = RH_RF95_MAX_MESSAGE_LEN;
    if (lora.receiveMessage((char *)loraBuffer, len))
    {
        processAndRespondToMessage((const char *)loraBuffer);
    }
#endif
}

void loop()
{
    digitalWrite(Config::LED_PIN, HIGH);
    handleLoraIncomingMessages();
#ifdef LORA
    if ((systemState.pinStateChanged) || (millis() - systemState.previousMillis >= Config::STATUS_INTERVAL))
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