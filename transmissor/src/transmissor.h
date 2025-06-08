#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <Arduino.h>
#include "prefers.h"
#include "stats.h"
#ifdef ESP32
#include <WiFi.h>
#include <time.h>
#include <esp_task_wdt.h>
#elif ESP8266
#endif

#ifdef ESP8266
#include <ESPAsyncWiFiManager.h>
#elif ESP32
#include <WiFiManager.h>
#endif

#include "LoRaCom.h"
#include "logger.h"
#include "config.h"
#ifdef DISPLAY_ENABLED
#include "display_manager.h"
#endif
#ifdef WS
#include "html_tserver.h"
#endif
#include "device_info.h"
#include "system_state.h"

#ifdef ALEXA
#include "AlexaCom.h"
#endif

#include "LoRaInterface.h"
#include "queue_message.h"
#include "app_messages.h"

#ifdef SINRIC
#include "SinricCom.h"
#endif

#ifdef WIFI
#include "ws_logger.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
DNSServer dns;

#endif

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
#ifdef ALEXA
        for (auto &dev : alexaCom.alexaDevices)
        {
            if (strcmp(dev.uniqueName().c_str(), device_name) == 0)
            {
                // LoRaCom::sendCommand("status", "get", dev.tid);
            }
        }
#endif
    }

    static void alexaDeviceCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
    {
#ifdef ALEXA
        // const uint8_t alexaId = (uint8_t)device_id;
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

                LoRaCom::sendCommand(EVT_GPIO, state ? GPIO_ON : GPIO_OFF, dev.tid);

                break;
            }
        }
#endif
    }

    //----------------------- WiFi ----------------------
#ifdef WIFI
    void initWiFi()
    {

        WiFi.mode(WIFI_STA);
        AsyncWiFiManager wifiManager(&server, &dns);
        // wifiManager.resetSettings();
        wifiManager.setConnectTimeout(Config::WIFI_TIMEOUT_S);
        wifiManager.setConfigPortalTimeout(Config::WIFI_TIMEOUT_S);

        if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
        {
            Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi - Reiniciando");
            safeRestart();
        }

        delay(500); // Estabilização após conexão

        WSLogger::initWs(server);
        server.begin();
        Logger::info("Servidor Web iniciado na porta 80");

        systemState.setWifiStatus(true);

        char ipBuffer[16];

        snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
        Logger::log(LogLevel::INFO, ipBuffer);
    }
#endif

    //----------------------- NTP ----------------------
#ifdef WIFI
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
            systemState.startedISODate = DeviceInfo::getISOTime().substring(11, 18);
        }
    }
#endif

    void safeRestart()
    {
        Logger::log(LogLevel::INFO, "Reinício seguro iniciado");
        delay(1000);
        // ESP.restart();
    }

public:
    // ========== Main Setup ==========
    void setup()
    {
        // Configurar watchdog
#ifdef ESP32
        esp_task_wdt_init(60, true);
#endif
        // Inicialização segura da Serial
        Serial.begin(Config::SERIAL_BAUD);
        uint32_t serialTimeout = millis();
        while (!Serial && (millis() - serialTimeout < 5000))
            ;
#ifdef WIFI
        initWiFi();

        initNTP();
#endif

#ifdef DEBUG_ON
        Serial.println("\n\nStarting with debug...");
// Enable detailed crash reports
#ifdef ESP32
        esp_log_level_set("*", ESP_LOG_VERBOSE);
#endif
#endif
        Serial.println(F("Iniciando sistema..."));

#ifdef ESP32
        try
        {
#endif
            Prefers::restoreRegs();

            if (!LoRaCom::initialize())
            {
                Logger::log(LogLevel::ERROR, "Falha na inicialização do LoRa");
            }
#ifdef DISPLAY_ENABLED
            displayManager.initialize();
#endif
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

#ifdef DISPLAY_ENABLED
            displayManager.update();
#endif
            systemState.setDiscovery(true);
#ifdef ESP32
        }
        catch (const std::exception &e)
        {
            Logger::log(LogLevel::WARNING, e.what());
            safeRestart();
        }
#endif
    }

    long ultimoReceived = 0;

    void logLivre()
    {
        if (millis() - ultimoReceived > 59000)
        {
            // initLora();
            ultimoReceived = millis();
            Logger::info("Tempo de atividade: %lu segundos", millis() / 1000);
#ifdef ESP32
            Logger::log(LogLevel::DEBUG, "Memória livre: %d bytes", ESP.getFreeHeap());
#endif
            Logger::info("Rxs: %d ps", stats.ps());
            stats.print();
        }
    }
    // ========== Main Loop ==========
    void loop()
    {
#ifdef ESP32
        esp_task_wdt_reset(); // Reset do watchdog
#endif
        if (primeiraVez)
        {
            Logger::log(LogLevel::VERBOSE, "Loop iniciado");
        }

        LoRaCom::handle();
        logLivre();
        if ((presentationCount < 3) && (millis() - updatePressentation > 10000) && systemState.isDiscovering())
        {
            LoRaCom::sendCommand(EVT_PRESENTATION, "", 0xFF);
            updatePressentation = millis();
            if (presentationCount++ > 2)
            {
                systemState.setDiscovery(false);
            }
        }

        if (millis() - updatePressentation > 60 * 60 * 1000)
        { // Corrigido para 1 hora
            updatePressentation = millis();
            LoRaCom::sendCommand(EVT_RESET, "gw", 0xFF);
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

        if (primeiraVez)
        {
            Logger::log(LogLevel::VERBOSE, "Loop finalizado");
            primeiraVez = false;
        }
    }

protected:
    static void processIncoming(MessageRec *rec)
    {

        if (!rec->event)
        {
            return;
        }

        Logger::info("Handled from: %d, %s|%s", rec->from, rec->event, rec->value);

        uint8_t tid = rec->from;
        if ((tid == 0) || (tid == 0xFF) || (rec->to == 0xFF))
        {
            return; // Descarta pacotes próprios ou inválidos
        }

        if (strstr(rec->event, EVT_ACK) != NULL || strstr(rec->event, EVT_NAK) != NULL)
        {
            Logger::info(rec->event);
            return;
        }

        bool handled = false;
        DeviceInfoData data;
        data.tid = tid;

        if (strstr(rec->event, EVT_STATUS) != NULL)
        {
            data.event = rec->event;
            data.value = rec->value;
            data.lastSeen = millis(); // DeviceInfo::getISOTime();

            DeviceInfo::updateDeviceList(data.tid, data);

            systemState.updateState(rec->value);

#ifdef ALEXA
            alexaCom.aliveOffLineAlexa();
            if (DeviceInfo::indexOf(tid) >= 0)
            {
                alexaCom.updateStateAlexa(DeviceInfo::findName(tid), rec->value);
            }
#endif
            handled = true;
        }
        else if (strcmp(rec->event, EVT_PRESENTATION) == 0)
        {
            if (systemState.isDiscovering())
            {
                DeviceRegData reg;
                reg.tid = tid;
                reg.name = rec->value;

                DeviceInfo::updateRegList(tid, reg);

                Prefers::saveRegs();

#ifdef ALEXA
                alexaCom.addDevice(tid, rec->value);
#endif
                handled = true;
            }
            else
            {
                Logger::log(LogLevel::ERROR, "Não está em modo discovery");
            }
        }
#ifdef DISPLAY_ENABLED
        if (handled)
        {
            displayManager.showMessage(rec->event); // showEvent(tid, rec->event, rec->value);
        }
#endif
        LoRaCom::ack(handled, tid);
    }
};

App app;

#endif // TRANSMISSOR_H