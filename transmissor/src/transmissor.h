#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <ArduinoJson.h>
#ifdef ESP32
#include "display_manager.h"
#elif ESP8266
#include "ESP8266WebServer.h"
#include "ESP8266WiFi.h"
#include "ESP8266httpUpdate.h"
#endif

#ifndef __AVR__
#include <WiFiManager.h>
#include <time.h>
#endif
#include "LoRaCom.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <map>
#include <pgmspace.h> // Use pgmspace.h for ESP8266
#elif ESP32
#include <WiFi.h>
#include <map>
#include "prefers.h"
#endif

#include <HTTPClient.h>
// #include <ArduinoJson.h>

#include "transmissor.h"
#include "logger.h"
#include "config.h"
#ifdef WS
#include "html_tserver.h"
#endif
#include "LoRaCom.h"
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

//--------------------------------------------------------------------------
// auth: AL
//--------------------------------------------------------------------------
class App
{
private:
    bool primeiraVez = true;
    long updatePressentation = 0;
    uint8_t presentationCount = 0;

    //-----------------------
    // ALEXA
    // ----------------------
    void discoverableCallback(bool discoverable)
    {
        // espalexa.setDiscoverable(discoverable);
        Logger::log(LogLevel::INFO, ("Alexa Discoverable " + String(discoverable ? "OK" : "OFF")).c_str());
    }

    static void alexaOnGetCallback(const char *device_name)
    {
        for (auto &dev : alexaCom.alexaDevices)
        {
            if (dev.uniqueName().equals(device_name))
            {
                LoRaCom::sendCommand("status", "get", dev.tid);
            }
        }
    }

    static void alexaDeviceCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
    {
#ifdef ALEXA
        const uint8_t alexaId = (uint8_t)device_id;
        Logger::log(LogLevel::DEBUG, String("Callback da Alexa: " + String(device_name)).c_str());
        for (auto &dev : alexaCom.alexaDevices)
        {
            Serial.print(dev.uniqueName());
            if (dev.uniqueName().equals(device_name))
            {
                // const bool state = (value > 0);
                Logger::info("Alexa(" + String(dev.alexaId) + "): " + dev.uniqueName() + " command: " + String(state ? "ON" : "OFF"));
                LoRaCom::sendCommand("gpio", state ? "on" : "off", dev.tid);
                delay(50);
                LoRaCom::sendCommand("status", "get", dev.tid);
                break;
            }
        }
#endif
    }
//-----------------------
// Inicialização
//-----------------------
#if defined(ESP8266) || defined(ESP32)
    void initWiFi()
    {
        WiFiManager wifiManager;
        wifiManager.setTimeout(Config::WIFI_TIMEOUT_S);

        if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
        {
            displayManager.message("Reiniciando: sem wifi");
            Logger::log(LogLevel::ERROR, F("Falha ao conectar WiFi"));
            delay(10000);
            ESP.restart();
        }

        systemState.setWifiStatus(true);
        char ipBuffer[16]; // Buffer to store IP address as a string
        snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
        Logger::log(LogLevel::INFO, ipBuffer);
    }
#endif

#if defined(ESP8266) || defined(ESP32)
    void initNTP()
    {
        configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
        Logger::log(LogLevel::INFO, F("Sincronizando com NTP..."));
        struct tm timeinfo;

        if (!getLocalTime(&timeinfo))
        {
            return;
        }
    }
#endif

public:
    // ========== Main Setup ==========
    void setup()
    {

#ifdef DEBUG_ON
        Logger::setLogLevel(LogLevel::VERBOSE);
#endif
        Serial.begin(Config::SERIAL_BAUD);
        while (!Serial)
            ;
        Logger::log(LogLevel::INFO, F("Iniciando sistema..."));
        Prefers::restoreRegs();

#if defined(ESP8266) || defined(__AVR__)
        Logger::log(LogLevel::WARNING, F("ESP8266 não possui display."));
#else
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
#endif

#if defined(ESP32) || defined(ESP8266)
        initWiFi();
        initNTP();
#endif

        if (!LoRaCom::initialize())
        {
            Logger::log(LogLevel::ERROR, F("Falha crítica no LoRa - reiniciando"));
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

#endif
        alexaCom.onGetCallback(alexaOnGetCallback);

#ifdef SINRIC
        sinricCom.setup();
#endif
        displayManager.updateDisplay();
        systemState.setDiscovery(true);
    }

    // ========== Main Loop ==========
    void loop()
    {
        if (primeiraVez)
        {
            Logger::log(LogLevel::VERBOSE, F("Loop iniciado"));
        }

        LoRaCom::handle(); // precisa pedir leitura rapida

        if ((presentationCount < 3) && (millis() - updatePressentation > 10000) && systemState.isDiscovering())
        {
            LoRaCom::sendCommand("presentation", "get", 0xFF);
            updatePressentation = millis();
            if (presentationCount++ > 2)
                systemState.setDiscovery(false);
        }
        if (millis() - updatePressentation > 60 * 60 * 60)
        {
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
        bool handled = false;
        uint8_t buf[Config::MESSAGE_LEN + 1]; // +1 para garantir espaço para null-terminator
        memset(buf, 0, sizeof(buf));
        uint8_t len = Config::MESSAGE_LEN;
        bool rt = loraInstance->receiveMessage(buf, len);
        if (!rt)
        {
            Logger::log(LogLevel::INFO, F("Não pegou ou descartou"));
            return;
        }

        uint8_t tid = loraInstance->headerFrom();
        if ((tid == 0) && (loraInstance->headerTo() == 0xFF)) // descarta pacotes proprios.
            return;

        if ((tid == 0) || (tid == 0xFF))
            return;

        // Remove caracteres não imprimíveis antes de desserializar
        for (uint8_t i = 0; i < len; i++)
        {
            if (buf[i] < 32 || buf[i] > 126)
            {
                buf[i] = ' '; // Substitui caracteres inválidos por espaço
            }
        }

        char msg[Config::MESSAGE_LEN];
        strcpy(msg, (char *)buf);

        if ((strstr(msg, "ack") != NULL) || (strstr(msg, "nak") != NULL))
        {
            // não é um dado do protocolo alto (ACK,NAK)
            Logger::info("Detectado " + String(msg));
            return;
        }

        if (strchr(msg, '|') == NULL)
        {
            LoRaCom::ack(false, loraInstance->headerFrom());
            Logger::log(LogLevel::ERROR, F("Descartado pacote LoRa inválido (não tem '|')"));
            Logger::log(LogLevel::ERROR, (const char *)buf);
            return;
        }

        const char *event = strtok(msg, "|");
        const char *value = strtok(NULL, "|");

        if (event)
        {
            if (strstr(event, "status") != NULL)
            {
                Logger::info(String(PSTR("Status dectado: " + String(value))));
                DeviceInfoData data;
                data.tid = tid;
                data.event = event;
                data.value = value;
                data.name = DeviceInfo::findName(tid);
                data.name.toLowerCase();
                data.lastSeenISOTime = DeviceInfo::getISOTime();
                data.rssi = loraInstance->packetRssi();
                DeviceInfo::updateDeviceList(data.tid, data);
                LoRaCom::ack(true, loraInstance->headerFrom());
                systemState.updateState(value);

#ifdef ALEXA

                alexaCom.aliveOffLineAlexa();
                alexaCom.updateStateAlexa(data.uniqueName(), value);
#endif
                //}
                handled = true;
            }
            else if (strcmp(event, "presentation") == 0)
            {
                Logger::info("Se apresentando: " + String(value));

                if (systemState.isDiscovering())
                {
                    LoRaCom::ack(true, loraInstance->headerFrom());
                    DeviceRegData reg;
                    reg.tid = tid;

                    reg.name = value;
                    reg.name.toLowerCase();
                    DeviceInfo::updateRegList(tid, reg);
                    Prefers::saveRegs();
                    displayManager.info("Encontrei '" + String(reg.name) + "'");

#ifdef ALEXA
                    alexaCom.addDevice(tid, value);
#endif
                    handled = true;
                }
                else
                {
                    Logger::log(LogLevel::ERROR, F("Não esta em espera para novo dispositivo"));
                }
            }
            if (handled)
                displayManager.setEvent(loraInstance->headerFrom(), event, value);
        }
        LoRaCom::ack(handled, loraInstance->headerFrom());
    }
};

App app;

#endif // TRANSMISSOR_H