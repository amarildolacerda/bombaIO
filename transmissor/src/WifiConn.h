#pragma once

#include "ESPAsyncWiFiManager.h"
#include "ESPAsyncWebServer.h"
#include "Arduino.h"
#include "WiFi.h"
#include "config.h"

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
public:
    bool setup()
    {
        bool result = false;

        return result;
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
