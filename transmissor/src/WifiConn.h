#pragma once

#include "ESPAsyncWiFiManager.h"
#include "ESPAsyncWebServer.h"
#include "Arduino.h"
#include "WiFi.h"
#include <ezTime.h>

#include "config.h"
#include "SystemState.h"
#include "DeviceInfo.h"
#include "AlexaCom.h"
#include "ws_logger.h"
#ifdef WIFI
#include "ws_logger.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
DNSServer dns;

AsyncWiFiManager wifiManager(&server, &dns);

#endif

AlexaCallbackType alexaCallbackFn;

class WiFiConn
{
private:
    Timezone myTZ;

    void initWiFi()
    {
        wifiManager.setConfigPortalTimeout(120);
        wifiManager.setConnectTimeout(30);
        wifiManager.setDebugOutput(true);
        wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        wifiManager.autoConnect(Config::TERMINAL_NAME);

        systemState.isConnected = WiFi.isConnected();

        if (systemState.isConnected)
        {
            WSLogger::initWs(server);
        }
    }
    void initNTP()
    {
        waitForSync();
        myTZ.setLocation();

        systemState.startedISODateTime = getISOTime();
    }

public:
    bool setup(AlexaCallbackType alexaCallback)
    {
        alexaCallbackFn = alexaCallback;
        initWiFi();
        if (systemState.isConnected)
        {
            initNTP();
            Logger::log(LogLevel::INFO, "WiFi conectado: %s", WiFi.SSID().c_str());
            systemState.isRunning = true;
            systemState.isConnected = true;
            systemState.isInitialized = true;
        }
        else
        {
            Logger::log(LogLevel::ERROR, "Falha ao conectar ao WiFi");
            systemState.isRunning = false;
            systemState.isConnected = false;
            systemState.isInitialized = false;
        }
        initAlexa();

        return systemState.isConnected;
    }
    void initAlexa()
    {
        alexaCom.setup(&server, [](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                       {
                           if (alexaCallbackFn)
                           {
                               alexaCallbackFn(device_id, device_name, state, value);
                           }
                           // Handle Alexa device callback
                           Logger::log(LogLevel::INFO, "Alexa Device Callback: ID: %d, Name: %s, State: %d, Value: %d", device_id, device_name, state, value); });
    }
    void loop()
    {
        alexaCom.loop();
    }
    String getISOTime(String format = "")
    {
#define baseTime "1970-01-01T00:00:00Z"
        if (WiFi.isConnected())
        {
            if (format.isEmpty())
            {
                return UTC.dateTime(ISO8601); // Use default format if none provided
            }
            return myTZ.dateTime(format);
        }

        else
        {
            return baseTime;
        }
    }
};

static WiFiConn wifiConn;
