#include "Arduino.h"
#include "config.h"
#include "app_messages.h"
#include "AppProcess.h"

#ifdef __AVR__
#include "RH_RF95.h"
#include <SoftwareSerial.h>
#include "LoRaRF95.h"
#endif

#ifdef HELTEC
#include "heltec.h"
#include "LoRa32.h"
#ifdef WIFI
#include "WiFIManager.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
#include "ws_logger.h"
#endif

#ifdef DISPLAY_ENABLED
#include "display_mgr.h"
#endif

#elif TTGO
#include "LoRa32.h"
#ifdef WIFI
#include "WiFIManager.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
#include "ws_logger.h"
#endif
#endif

#include "logger.h"
#include "queue_message.h"

#ifndef SAVEMEMORY
#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL

typedef void (*CallbackFunction)();
CallbackFunction callback;

void initCallback();

#endif

class App
{
private:
public:
#if defined(WIFI)
    void initWiFi()
    {

        WiFi.mode(WIFI_STA);
        WiFiManager wifiManager;
        // wifiManager.resetSettings();
        wifiManager.setConnectTimeout(360);
        wifiManager.setConfigPortalTimeout(360);

        if (!wifiManager.autoConnect(Config::TERMINAL_NAME))
        {
            Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi - Reiniciando");
        }

        delay(500); // Estabilização após conexão

        WSLogger::initWs(server);
        server.begin();
        Logger::info("Servidor Web iniciado na porta 80");

        //        systemState.setWifiStatus(true);

        char ipBuffer[16];

        snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
        Logger::log(LogLevel::INFO, ipBuffer);
    }
#endif

    //----------------------- NTP ----------------------
#if defined(WIFI)
    void initNTP()
    {
        configTzTime(Config::TIMEZONE, Config::NTP_SERVER);
        Logger::log(LogLevel::INFO, "Sincronizando com NTP...");

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 5000))
        { // Timeout de 5 segundos
            Logger::log(LogLevel::WARNING, "Falha ao obter tempo NTP");
            return;
        }
        else
        {
            systemState.startedISODate = systemState.getISOTime().substring(11, 18);
        }
    }
#endif

    long sendUpdated = 0;

    void setup()
    {
        Serial.begin(Config::SERIAL_SPEED);
        while (!Serial)
            ;

#ifdef WIFI
        initWiFi();
        initNTP();
        WSLogger::initWs(server);
#endif

        lora.config = LORA_MED;
        lora.begin(Config::TERMINAL_ID, Config::LORA_BAND, Config::PROMISCUOS);
        if (!lora.connected)
        {
            Serial.print(F("LoRa failed to start"));
        };
#ifdef DISPLAY_ENABLED
        displayManager.initialize();
        displayManager.loraConnected = lora.connected;
#endif

        pinMode(Config::LED_PIN, OUTPUT);
        pinMode(Config::RELAY_PIN, OUTPUT);
#ifndef SAVEMEMORY
        initPinRelay();
#endif
        Logger::info("Pronto terminal %d - %s", Config::TERMINAL_ID, Config::TERMINAL_NAME);
    }

    long ultimoReceived = 0;

    void loop()
    {
        lora.loop();
        if (AppProcess::handle())
        {
            ultimoReceived = millis();
        }

        if (millis() - ultimoReceived > 60000)
        {
            ultimoReceived = millis();
            Logger::info("Tempo de atividade: %lu segundos", millis() / 1000);
#ifdef ESP32
            Logger::log(LogLevel::DEBUG, "Memória livre: %d bytes", ESP.getFreeHeap());
#endif
        }

#ifdef DISPLAY_ENABLED
        displayManager.ISOTime = systemState.getISOTime("%H:%M:%S");
        displayManager.rssi = lora.packetRssi();
        displayManager.handle();
#endif
    }

    void sendEvent(uint8_t tid, const char *event, const char *value)
    {
        CRITICAL_SECTION
        {
            lora.send(tid, event, value, Config::TERMINAL_ID);
#ifdef DISPLAY_ENABLED
            displayManager.showMessage("[SEND] " + String(event) + ": " + String(value));
#endif
        }
    }

#ifndef SAVEMEMORY
    void savePinState(bool state)
    {
        CRITICAL_SECTION
        {
#ifdef __AVR__
            EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
#else
            EEPROM.write(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
            EEPROM.commit();
#endif
        }
    }

    bool readPinState()
    {
        uint8_t val = 0;
        CRITICAL_SECTION
        {
            val = EEPROM.read(EEPROM_ADDR_PIN5_STATE);
        }
        return val == 1;
    }

    void initPinRelay()
    {
        bool savedState = false;
        pinMode(Config::RELAY_PIN, OUTPUT);
        savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);

        Logger::info(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF");
#ifdef ESP32
        initCallback();
#endif
    }
#endif
};

App app;

#ifndef SAVEMEMORY
void triggerCallback()
{
    CRITICAL_SECTION
    {
#ifndef SAVEMEMORY
        app.savePinState(digitalRead(Config::RELAY_PIN));
#endif
        AppProcess::setStatusChanged();
    }
}

static bool callbackInstaled = false;
void initCallback()
{
    if (callbackInstaled)
    {
        callbackInstaled = true;
        detachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN));
    }
    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), triggerCallback, CHANGE);
}
#endif