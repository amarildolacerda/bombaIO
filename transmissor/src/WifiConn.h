#pragma once

#include "ESPAsyncWiFiManager.h"
#include "ESPAsyncWebServer.h"
#include "Arduino.h"
#include "WiFi.h"
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
    void initWiFi()
    {
        wifiManager.setConfigPortalTimeout(120);
        wifiManager.setConnectTimeout(30);
        wifiManager.setDebugOutput(true);
        wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        systemState.isConnected = wifiManager.autoConnect(Config::TERMINAL_NAME);

        if (systemState.isConnected)
            WSLogger::initWs(server);
    }
    void initNTP()
    {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 5000))
        {
            Logger::log(LogLevel::WARNING, "Falha ao obter tempo NTP");
            return;
        }
        else
        {
            systemState.startedISODateTime = getISOTime();
        }
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
    String getISOTime()
    {
#define baseTime "1970-01-01T00:00:00Z"
        if (WiFi.isConnected())
        {
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo))
            {
                return baseTime;
            }

            char timeStr[25];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
            return String(timeStr);
        }

        else
        {
            return baseTime;
        }
    }
};

static WiFiConn wifiConn;
