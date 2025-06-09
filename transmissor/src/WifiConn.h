#pragma once

#include "ESPAsyncWiFiManager.h"
#include "ESPAsyncWebServer.h"
#include "Arduino.h"
#include "WiFi.h"
#include <ezTime.h>

#include "config.h"
#include "SystemState.h"
#include "DeviceInfo.h"
#include "ws_logger.h"
#ifdef WIFI
#include "ws_logger.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
DNSServer dns;

AsyncWiFiManager wifiManager(&server, &dns);

#endif

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
    bool setup()
    {
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
        return systemState.isConnected;
    }
    void loop()
    {
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
