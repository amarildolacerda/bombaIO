#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "LoRaCom.h"
#include "logger.h"
#include "device_info.h"
#include "system_state.h"
#include <WiFiManager.h>
#include "prefers.h"
#include "display_manager.h"

#ifdef ALEXA
#include "AlexaCom.h"
#endif

#ifdef WS
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
#endif

// Strings constantes em PROGMEM
const char PROGMEM str_system_starting[] = "System starting...";
const char PROGMEM str_first_loop[] = "First loop";
const char PROGMEM str_lora_init_failed[] = "LoRa init failed";
const char PROGMEM str_safe_restart[] = "Safe restart...";
const char PROGMEM str_wifi_failed[] = "WiFi failed - Restarting";
const char PROGMEM str_ntp_syncing[] = "Syncing NTP...";
const char PROGMEM str_ntp_failed[] = "NTP sync failed";
const char PROGMEM str_invalid_packet[] = "Invalid packet (no separator)";
const char PROGMEM str_alexa_cmd[] = "Alexa cmd: ";

// Comandos LoRa em PROGMEM
const char PROGMEM cmd_status[] = "status";
const char PROGMEM cmd_get[] = "get";
const char PROGMEM cmd_gpio[] = "gpio";
const char PROGMEM cmd_on[] = "on";
const char PROGMEM cmd_off[] = "off";
const char PROGMEM cmd_presentation[] = "presentation";
const char PROGMEM cmd_reset[] = "reset";
const char PROGMEM cmd_set[] = "set";

class App
{
private:
    bool firstLoop = true;
    unsigned long lastPresentationTime = 0;
    uint8_t presentationCount = 0;

    // Alexa callbacks
    static void alexaOnGet(const char *device_name)
    {
        for (auto &dev : alexaCom.devices)
        {
            if (strcmp(dev.uniqueName().c_str(), device_name) == 0)
            {
                char cmd_status_pgm[10];
                char cmd_get_pgm[10];
                strcpy_P(cmd_status_pgm, cmd_status);
                strcpy_P(cmd_get_pgm, cmd_get);
                LoRaCom::sendCommand(cmd_status_pgm, cmd_get_pgm, dev.tid);
                break;
            }
        }
    }

    static void alexaCallback(uint8_t device_id, const char *device_name, bool state, uint8_t value)
    {
#ifdef ALEXA
        for (auto &dev : alexaCom.devices)
        {
            if (dev.uniqueName().equals(device_name))
            {
                char log_msg[50];
                char alexa_cmd_pgm[20];
                strcpy_P(alexa_cmd_pgm, str_alexa_cmd);
                snprintf_P(log_msg, sizeof(log_msg), PSTR("%s%s%s"), alexa_cmd_pgm, device_name, (state ? " ON" : " OFF"));
                Logger::info(log_msg);

                char cmd_gpio_pgm[10];
                char cmd_on_pgm[10];
                char cmd_off_pgm[10];
                char cmd_status_pgm[10];
                char cmd_get_pgm[10];

                strcpy_P(cmd_gpio_pgm, cmd_gpio);
                strcpy_P(cmd_on_pgm, cmd_on);
                strcpy_P(cmd_off_pgm, cmd_off);
                strcpy_P(cmd_status_pgm, cmd_status);
                strcpy_P(cmd_get_pgm, cmd_get);

                LoRaCom::sendCommand(cmd_gpio_pgm, state ? cmd_on_pgm : cmd_off_pgm, dev.tid);
                delay(50);
                LoRaCom::sendCommand(cmd_status_pgm, cmd_get_pgm, dev.tid);
                break;
            }
        }
#endif
    }

    // WiFi initialization
#if defined(ESP8266) || defined(ESP32)
    void initWiFi()
    {
        WiFi.mode(WIFI_STA);
        WiFiManager wifiManager;
        wifiManager.setConnectTimeout(180);

        if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
        {
            char error_msg[30];
            strcpy_P(error_msg, str_wifi_failed);
            Logger::error(error_msg);
            safeRestart();
        }

        systemState.setWifiStatus(true);
        Logger::info(WiFi.localIP().toString().c_str());
    }
#endif

    // NTP initialization
#if defined(ESP8266) || defined(ESP32)
    void initNTP()
    {
        /*    char info_msg[20];
            strcpy_P(info_msg, str_ntp_syncing);
            Logger::info(info_msg);

            struct tm timeinfo;
            if (!getLocalTime(&timeinfo, 5000))
            {
                char warn_msg[20];
                strcpy_P(warn_msg, str_ntp_failed);
                Logger::warn(warn_msg);
            }
                */
    }
#endif

    void safeRestart()
    {
        char info_msg[20];
        strcpy_P(info_msg, str_safe_restart);
        Logger::info(info_msg);
        delay(1000);
        ESP.restart();
    }

    // Process incoming LoRa messages
    static void processMessage(LoRaInterface *lora)
    {
        if (!lora)
            return;

        uint8_t buf[Config::MESSAGE_LEN + 1] = {0};
        uint8_t len = Config::MESSAGE_LEN;

        if (!lora->receiveMessage(buf, len))
        {
            return;
        }

        uint8_t tid = lora->headerFrom();
        if (tid == 0 || tid == 0xFF)
            return;

        // Sanitize buffer
        for (uint8_t i = 0; i < len && i < Config::MESSAGE_LEN; i++)
        {
            if (buf[i] < 32 || buf[i] > 126)
                buf[i] = ' ';
        }
        buf[len] = '\0';

        char *separator = strchr((char *)buf, '|');
        if (!separator)
        {
            LoRaCom::ack(false, tid);
            char error_msg[30];
            strcpy_P(error_msg, str_invalid_packet);
            Logger::error(error_msg);
            return;
        }

        *separator = '\0';
        const char *event = (char *)buf;
        const char *value = separator + 1;

        bool handled = false;
        DeviceInfoData data;
        data.tid = tid;
        data.rssi = lora->packetRssi();

        char cmd_status_pgm[10];
        strcpy_P(cmd_status_pgm, cmd_status);

        if (strstr(event, cmd_status_pgm) != NULL)
        {
            data.event = event;
            data.value = value;
            data.name = DeviceInfo::findName(tid);
            data.lastSeenISOTime = DeviceInfo::getISOTime();
            DeviceInfo::updateDeviceList(data.tid, data);

#ifdef ALEXA
            alexaCom.checkOffline();
            alexaCom.updateState(data.name, value);
#endif
            handled = true;
        }

        char cmd_presentation_pgm[15];
        strcpy_P(cmd_presentation_pgm, cmd_presentation);

        if (strcmp(event, cmd_presentation_pgm) == 0 && systemState.isDiscovering())
        {
            DeviceRegData reg;
            reg.tid = tid;
            reg.name = value;
            DeviceInfo::updateRegList(tid, reg);
            Prefers::saveRegs();

#ifdef ALEXA
            alexaCom.addDevice(tid, value);
#endif
            handled = true;
        }

        if (handled)
        {
            displayManager.setEvent(tid, event, value);
        }
        LoRaCom::ack(handled, tid);
    }

public:
    void setup()
    {
        Serial.begin(Config::SERIAL_BAUD);
        while (!Serial && millis() < 5000)
            ;

        char info_msg[20];
        strcpy_P(info_msg, str_system_starting);
        Logger::info(info_msg);

        try
        {
            Prefers::restoreRegs();

#if defined(ESP32) || defined(ESP8266)
            initWiFi();
            initNTP();
#endif

            if (!LoRaCom::initialize())
            {
                char error_msg[30];
                strcpy_P(error_msg, str_lora_init_failed);
                throw std::runtime_error(error_msg);
            }

            LoRaCom::setReceiveCallback(processMessage);

#ifdef ALEXA
            alexaCom.setup(
#ifdef WS
                &server
#else
                NULL
#endif
                ,
                alexaCallback);
            alexaCom.onGetCallback(alexaOnGet);
#endif

            displayManager.updateDisplay();
            systemState.setDiscovery(true);
        }
        catch (const std::exception &e)
        {
            Logger::error(e.what());
            safeRestart();
        }
    }

    void loop()
    {
        if (firstLoop)
        {
            char debug_msg[20];
            strcpy_P(debug_msg, str_first_loop);
            Logger::debug(debug_msg);
            firstLoop = false;
        }

        LoRaCom::handle();

        // Send presentation every 10s (max 3 times)
        if (presentationCount < 3 && millis() - lastPresentationTime > 10000 && systemState.isDiscovering())
        {
            char cmd_presentation_pgm[15];
            char cmd_get_pgm[10];
            strcpy_P(cmd_presentation_pgm, cmd_presentation);
            strcpy_P(cmd_get_pgm, cmd_get);
            LoRaCom::sendCommand(cmd_presentation_pgm, cmd_get_pgm, 0xFF);
            lastPresentationTime = millis();
            if (presentationCount++ > 2)
            {
                systemState.setDiscovery(false);
            }
        }

        // Hourly reset
        if (millis() - lastPresentationTime > 3600000)
        {
            lastPresentationTime = millis();
            char cmd_reset_pgm[10];
            char cmd_set_pgm[10];
            strcpy_P(cmd_reset_pgm, cmd_reset);
            strcpy_P(cmd_set_pgm, cmd_set);
            LoRaCom::sendCommand(cmd_reset_pgm, cmd_set_pgm, 0xFF);
        }

#ifdef ALEXA
        alexaCom.loop();
#endif

        systemState.handle();
        displayManager.handle();
    }
};

App app;

#endif // TRANSMISSOR_H