#include "receptor.h"
#include "Arduino.h"
#include "config.h"
#ifndef x__AVR__
// #include "ArduinoJson.h"
#endif
#ifdef LORA
#include <LoRaRF95.h>
#endif
#include "logger.h"
#include "html_server.h"
#include "timestamp.h"

#ifdef X__AVR__
#include <avr/wdt.h>
long wdtUpdate = 0;
bool wdtEnable = true;
void WDT_ENABLE()
{
    wdt_enable(WDTO_15MS);
    // wdtEnable = true;
    // wdtUpdate = millis();
}
void WDT_RESET()
{
    wdt_reset();
    // wdtUpdate = millis();
}
void WDT_DISABLE()
{
    wdt_disable();
    // wdtEnable = false;
}
// #define WDT_ENABLE() wdt_enable(WDTO_30MS) // Habilita WDT com timeout de 4 segundos
// #define WDT_RESET() wdt_reset()            // Reinicia o contador do WDT ("alimenta o cachorro")
// #define WDT_DISABLE() wdt_disable()        // Desativa o WDT (use com cuidado)
#else
#define WDT_ENABLE()  // Nada em plataformas não-AVR
#define WDT_RESET()   // Nada em plataformas não-AVR
#define WDT_DISABLE() // Nada em plataformas não-AVR
#endif

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
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL      // 10 segundos para zerar contagem
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL // 1 minuto para 3 boots
#endif

bool sendStatus();
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

void delaySafe(unsigned long ms)
{
#ifdef __AVR__
    unsigned long start = millis();
    while (millis() - start < ms)
    {
        // WDT_RESET();
        delay(1);
    }
#else
    delay(ms);
#endif
}

#ifdef __AVR__
// Função para salvar o estado do pino 5 na EEPROM
void savePinState(bool state)
{
    EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
}

// Função para ler o estado do pino 5 da EEPROM
bool readPinState()
{
    return EEPROM.read(EEPROM_ADDR_PIN5_STATE) == 1;
}

// Função para inicializar o pino 5 com o estado salvo
void initPinRelay()
{
    pinMode(5, OUTPUT);
    bool savedState = readPinState();
    digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);
    Logger::info(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF");
}
#endif

#ifdef __AVR__
static bool waitingStatusACK = false;
bool waitAck(const uint32_t timeout)
{
    char msg[Config::MESSAGE_MAX_LEN];
    uint8_t len = sizeof(msg);

    if (lora.available(500))
    {
        if (lora.receiveMessage(msg, len))
        {
            if (strstr("ack;nak", msg) != NULL)
            {
                if (waitingStatusACK)
                {
                    waitingStatusACK = false;
                    systemState.pinStateChanged = false;
                }
                return true;
            }
            else
            {
                return processAndRespondToMessage(msg);
            }
        }
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
    Serial.print(F("Boot Count: "));
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
        Logger::log(LogLevel::WARNING, F("Apresentacao!"));
        // Modo identificação: envia apresentação continuamente
        zeraContadorReinicio();
        for (int i = 0; i < 3; ++i)
        {
            Serial.println(F("presentation"));
            sendPresentation(1);
            if (waitAck(500))
            {
                break;
            }
        }
    }
}
#endif

// Define systemState struct and global instance

void setup()
{
#ifdef __AVR__
    // Desabilita WDT imediatamente para evitar reset acidental durante a inicialização
    // MCUSR &= ~(1 << WDRF); // Limpa o flag de reset do WDT
    WDT_DISABLE();
#endif

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
    initPinRelay();

    systemState.lastPinState = digitalRead(Config::RELAY_PIN);

#ifdef ESP8266
    initHtmlServer();
#endif

    digitalWrite(Config::LED_PIN, LOW);
    systemState.pinStateChanged = true;

#ifdef __AVR__
    WDT_ENABLE(); // Habilita o Watchdog após a inicialização
#endif
}

void formatMessage(uint8_t tid, const char *event, const char *value = "")
{
#ifdef LORA
    memset(messageBuffer, 0, sizeof(messageBuffer));
    snprintf(messageBuffer, sizeof(messageBuffer),
             "%s|%s", event, value);
    /* snprintf(messageBuffer, sizeof(messageBuffer),
              "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}",
              Config::TERMINAL_NAME, event, value);
    */
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
    if (loraActive)
    {
        bool ackReceived = false;
        for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
        {
            String attemptMessage = String(attempt + 1);
            sendFormattedMessage(0, "presentation", Config::TERMINAL_NAME);
            if (waitAck(200))
                return;
        }
    }
#endif
}

bool sendStatus()
{
    // WDT_RESET();

    bool currentState = digitalRead(Config::RELAY_PIN);
    savePinState(currentState);
    const char *status = currentState == HIGH ? "on" : "off";
    sendFormattedMessage(0x00, "status", status);
    systemState.lastPinState = currentState;
    systemState.pinStateChanged = true;
    waitingStatusACK = true;
    systemState.previousMillis = millis();
    return waitAck(200);
    // return lora.available(200);
}

bool processAndRespondToMessage(const char *message)
{

    uint8_t tid = 0; // Placeholder for terminal ID
    bool handled = false;
    if (message == nullptr)
    {
        return false;
    }
    digitalWrite(Config::LED_PIN, HIGH);

    if (strstr("ack;nak;", message) != NULL)
    {
        // nao responde ACK nem NAK
        if (waitingStatusACK)
        {
            waitingStatusACK = false;
            systemState.pinStateChanged = false;
        }
        int rssi = lora.getLastRssi();
        if (rssi != 0)
        {
            Logger::log(LogLevel::WARNING, String("RSSI: " + String(rssi) + " dBm").c_str());
        }
        return true;
    }
    else
    {
        char buffer[sizeof(message)];
        strcpy(buffer, message);
        // Divide a string usando '|' como delimitador
        const char *event = strtok(buffer, "|"); // Pega a primeira parte ("status")
        const char *value = strtok(NULL, "|");   // Pega a segunda parte ("value")
        if ((event == NULL) || (value == NULL))
            return false;

        if (strcmp(event, "gpio") == 0)
        {
            if (strstr("on;off", value) != NULL)
            {
                digitalWrite(Config::RELAY_PIN, (strcmp(event, "on") == 0) ? HIGH : LOW);
                systemState.pinStateChanged = true;

                sendStatus();
                handled = true;
            }
            else if (strcmp(value, "toggle") == 0)
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                systemState.pinStateChanged = true;
                sendStatus();
                handled = true;
            }
        }
        else if (strcmp(event, "presentation") == 0)
        {
            systemState.mustPresentation = true;
            handled = true;
        }
        else if (strcmp(event, "ping") == 0)
        {
            lora.sendMessage(tid, "pong");
            // nao responde com ack nem nak
            return true;
        }
        else if (strcmp(event, "time") == 0)
        {
            if (timestamp.isValidTimeFormat(value))
            {
                timestamp.setCurrentTime(value);
                handled = true;
            }
            else
            {
                Serial.println(F("Erro: Timestamp formato de tempo inválido"));
            }
        }
    }

    ack(handled, tid);
    return handled;
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
    if (lora.available(200))
    {
        uint8_t len = sizeof(loraBuffer);
        if (lora.receiveMessage((char *)loraBuffer, len))
        {
            processAndRespondToMessage((const char *)loraBuffer);
        }
    }
#endif
}

long checaZeraContador = millis();
bool zerou = false;
void (*restart)(void) = 0; // Declaração de ponteiro de função para endereço 0 (reset)

void loop()
{

    if ((!zerou) && (millis() - checaZeraContador > 60000))
    { // depois de um minuto rodando, reset o contador de reinicio;
        zeraContadorReinicio();
        zerou = true;
    }

    if ((!loraActive))
    {
        Serial.print(F("LoRa inativo"));
        digitalWrite(Config::LED_PIN, HIGH);
        delay(1000);
        digitalWrite(Config::LED_PIN, LOW);
        delay(500);

        loraActive = lora.reactive();
        return;
    }

    // WDT_DISABLE(); // Reinicia o contador do WDT (evita reset não desejado)

#ifdef LORA
    handleLoraIncomingMessages();
    unsigned long status_internal = (systemState.pinStateChanged ? 5000 : Config::STATUS_INTERVAL);
    if ((millis() - systemState.previousMillis) >= status_internal)
    {
        systemState.previousMillis = millis();
        sendStatus();
    }

#endif

#ifdef ESP8266
    // WDT_RESET();
    processHtmlServer();
#endif
    digitalWrite(Config::LED_PIN, LOW);
    // WDT_ENABLE();
}