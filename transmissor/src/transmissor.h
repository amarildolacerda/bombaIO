#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "prefers.h"
#ifdef ESP32
#include "display_manager.h"
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#elif ESP8266
#include "ESP8266WebServer.h"
#include "ESP8266WiFi.h"
#include "ESP8266httpUpdate.h"
#endif

#ifndef __AVR__
#include <WiFiManager.h>
#endif

#include "LoRaCom.h"
#include "logger.h"
#include "config.h"
#ifdef WS
#include "html_tserver.h"
#endif
#include "device_info.h"
#include "system_state.h"

#ifdef TTGO
#include "display_manager.h"
#endif

#ifdef ALEXA
#include "AlexaCom.h"
#endif

#include "LoRaInterface.h"

#ifdef SINRIC
#include "SinricCom.h"
#endif

#ifdef WS
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
#endif

// Buffer estático para mensagens
static char staticMsgBuffer[Config::MESSAGE_LEN + 1];

class App
{
private:
    bool primeiraVez = true;
    long updatePressentation = 0;
    uint8_t presentationCount = 0;

    //----------------------- ALEXA ----------------------
    void discoverableCallback(bool discoverable)
    {
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "Alexa Discoverable %s", discoverable ? "OK" : "OFF");
        Logger::log(LogLevel::INFO, logMsg);
    }

    static void alexaOnGetCallback(const char *device_name)
    {
        for (auto &dev : alexaCom.alexaDevices)
        {
            if (strcmp(dev.uniqueName().c_str(), device_name) == 0)
            {
                LoRaCom::sendCommand("status", "get", dev.tid);
            }
        }
    }

    static void alexaDeviceCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
    {
#ifdef ALEXA
        const uint8_t alexaId = (uint8_t)device_id;
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "Callback da Alexa: %s", device_name);
        Logger::log(LogLevel::DEBUG, logMsg);

        for (auto &dev : alexaCom.alexaDevices)
        {
            if (dev.uniqueName().equals(device_name))
            {
                snprintf(logMsg, sizeof(logMsg), "Alexa(%d): %s command: %s",
                         dev.alexaId, dev.uniqueName().c_str(), state ? "ON" : "OFF");
                Logger::info(logMsg);

                LoRaCom::sendCommand("gpio", state ? "on" : "off", dev.tid);
                delay(50);
                LoRaCom::sendCommand("status", "get", dev.tid);
                break;
            }
        }
#endif
    }

    //----------------------- WiFi ----------------------
#if defined(ESP8266) || defined(ESP32)
    void initWiFi()
    {
        WiFi.mode(WIFI_STA);
        WiFiManager wifiManager;
        wifiManager.setConnectTimeout(Config::WIFI_TIMEOUT_S);
        wifiManager.setConfigPortalTimeout(Config::WIFI_TIMEOUT_S);

        if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
        {
            Logger::log(LogLevel::ERROR, F("Falha ao conectar WiFi - Reiniciando"));
            safeRestart();
        }

        delay(500); // Estabilização após conexão
        systemState.setWifiStatus(true);

        char ipBuffer[16];
        snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
        Logger::log(LogLevel::INFO, ipBuffer);
    }
#endif

    //----------------------- NTP ----------------------
#if defined(ESP8266) || defined(ESP32)
    void initNTP()
    {
        configTzTime(Config::TIMEZONE, Config::NTP_SERVER);
        Logger::log(LogLevel::INFO, F("Sincronizando com NTP..."));

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 5000))
        { // Timeout de 5 segundos
            Logger::log(LogLevel::WARNING, F("Falha ao obter tempo NTP"));
            return;
        }
    }
#endif

    void safeRestart()
    {
        Logger::log(LogLevel::INFO, F("Reinício seguro iniciado"));
        delay(1000);
        ESP.restart();
    }

public:
    // ========== Main Setup ==========
    void setup()
    {
        // Configurar watchdog
        esp_task_wdt_init(60, true);

        // Inicialização segura da Serial
        Serial.begin(Config::SERIAL_BAUD);
        uint32_t serialTimeout = millis();
        while (!Serial && (millis() - serialTimeout < 5000))
            ;

#ifdef DEBUG_ON
        Serial.println("\n\nStarting with debug...");
        // Enable detailed crash reports
        esp_log_level_set("*", ESP_LOG_VERBOSE);
        Logger::setLogLevel(LogLevel::VERBOSE);
#endif
        Serial.println(F("Iniciando sistema..."));

        try
        {
            Prefers::restoreRegs();

#if defined(ESP32)
            if (display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS))
            {
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println("Iniciando...");
                display.display();
            }
            else
            {
                Logger::log(LogLevel::ERROR, F("Falha ao iniciar display OLED"));
            }
#elif defined(ESP8266)
            Logger::log(LogLevel::WARNING, F("ESP8266 não possui display."));
#endif

#if defined(ESP32) || defined(ESP8266)
            initWiFi();
            initNTP();
#endif

            if (!LoRaCom::initialize())
            {
                throw std::runtime_error("Falha na inicialização do LoRa");
            }

            LoRaCom::setReceiveCallback(processIncoming);

#ifdef WS
            HtmlServer::initWebServer(&server);
            HtmlServer::begin();
#endif

#ifdef ALEXA
            alexaCom.setup(
#ifdef WS
                &server
#else
                NULL
#endif
                ,
                alexaDeviceCallback);
            alexaCom.onGetCallback(alexaOnGetCallback);
#endif

#ifdef SINRIC
            sinricCom.setup();
#endif

            displayManager.updateDisplay();
            systemState.setDiscovery(true);
        }
        catch (const std::exception &e)
        {
            Logger::log(LogLevel::CRITICAL, e.what());
            safeRestart();
        }
    }

    // ========== Main Loop ==========
    void loop()
    {
        esp_task_wdt_reset(); // Reset do watchdog

        if (primeiraVez)
        {
            Logger::log(LogLevel::VERBOSE, F("Loop iniciado"));
        }

        LoRaCom::handle();

        if ((presentationCount < 3) && (millis() - updatePressentation > 10000) && systemState.isDiscovering())
        {
            LoRaCom::sendCommand("presentation", "get", 0xFF);
            updatePressentation = millis();
            if (presentationCount++ > 2)
            {
                systemState.setDiscovery(false);
            }
        }

        if (millis() - updatePressentation > 60 * 60 * 1000)
        { // Corrigido para 1 hora
            updatePressentation = millis();
            LoRaCom::sendCommand("reset", "set", 0xFF);
        }

#ifdef WS
        HtmlServer::process();
#endif

#ifdef ALEXA
        alexaCom.loop();
#endif

        systemState.handle();

#ifdef SINRIC
        sinricCom.loop();
#endif

        displayManager.handle();

        if (primeiraVez)
        {
            Logger::log(LogLevel::VERBOSE, F("Loop finalizado"));
            primeiraVez = false;
        }
    }

protected:
    static void processIncoming(LoRaInterface *loraInstance)
    {
        if (!loraInstance)
            return;

        uint8_t buf[Config::MESSAGE_LEN + 1];
        memset(buf, 0, sizeof(buf)); // Clear buffer first
        uint8_t len = Config::MESSAGE_LEN;

        if (!loraInstance->receiveMessage(buf, len))
        {
            Logger::log(LogLevel::INFO, F("Não pegou ou descartou"));
            return;
        }

        uint8_t tid = loraInstance->headerFrom();
        if ((tid == 0) || (tid == 0xFF) || (loraInstance->headerTo() == 0xFF))
        {
            return; // Descarta pacotes próprios ou inválidos
        }

        // Sanitiza o buffer
        for (uint8_t i = 0; i < len && i < Config::MESSAGE_LEN; i++)
        {
            if (buf[i] < 32 || buf[i] > 126)
            {
                buf[i] = ' ';
            }
        }
        buf[len] = '\0'; // Garante terminação nula

        // Usa buffer estático para evitar alocação na pilha
        strncpy(staticMsgBuffer, (char *)buf, Config::MESSAGE_LEN);
        staticMsgBuffer[Config::MESSAGE_LEN] = '\0';

        if (strstr(staticMsgBuffer, "ack") != NULL || strstr(staticMsgBuffer, "nak") != NULL)
        {
            char logMsg[64];
            snprintf(logMsg, sizeof(logMsg), "Detectado %s", staticMsgBuffer);
            Logger::info(logMsg);
            return;
        }

        char *separator = strchr(staticMsgBuffer, '|');
        if (!separator)
        {
            LoRaCom::ack(false, tid);
            Logger::log(LogLevel::ERROR, F("Pacote inválido (sem separador '|')"));
            return;
        }

        *separator = '\0'; // Separa evento e valor
        const char *event = staticMsgBuffer;
        const char *value = separator + 1;

        bool handled = false;
        DeviceInfoData data;
        data.tid = tid;
        data.rssi = loraInstance->packetRssi();

        if (strstr(event, "status") != NULL)
        {
            char logMsg[128];
            snprintf(logMsg, sizeof(logMsg), "Status detectado: %s", value);
            Logger::info(logMsg);

            data.event = event;
            data.value = value;
            // data.name = DeviceInfo::findName(tid);
            data.lastSeenISOTime = DeviceInfo::getISOTime();
            DeviceInfo::updateDeviceList(data.tid, data);

            systemState.updateState(value);

#ifdef ALEXA
            alexaCom.aliveOffLineAlexa();
            alexaCom.updateStateAlexa(DeviceInfo::findName(tid), value);
#endif
            handled = true;
        }
        else if (strcmp(event, "presentation") == 0)
        {
            char logMsg[128];
            snprintf(logMsg, sizeof(logMsg), "Se apresentando: %s", value);
            Logger::info(logMsg);

            if (systemState.isDiscovering())
            {
                DeviceRegData reg;
                reg.tid = tid;
                reg.name = value;
                DeviceInfo::updateRegList(tid, reg);
                Prefers::saveRegs();

                char displayMsg[64];
                snprintf(displayMsg, sizeof(displayMsg), "Encontrei '%s'", reg.name.c_str());
                displayManager.info(displayMsg);

#ifdef ALEXA
                alexaCom.addDevice(tid, value);
#endif
                handled = true;
            }
            else
            {
                Logger::log(LogLevel::ERROR, F("Não está em modo discovery"));
            }
        }

        if (handled)
        {
            displayManager.setEvent(tid, event, value);
        }
        LoRaCom::ack(handled, tid);
    }
};

App app;

#endif // TRANSMISSOR_H