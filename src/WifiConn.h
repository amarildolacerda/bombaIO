#pragma once

#ifdef WIFI

#include "Arduino.h"
#include "config.h"
#include "SystemState.h"
#include "ws_logger.h"

// Conditional includes
#ifdef GATEWAY
#include "DeviceInfo.h"
#endif

#ifdef ALEXA
#include "AlexaCom.h"
#endif

// WiFi related includes
#include "ESPAsyncWiFiManager.h"
#include "ESPAsyncWebServer.h"
#include "WiFi.h"
#include <ezTime.h>

#ifdef WS
#include "html_tserver.h"
#endif

class WiFiConn
{
private:
    Timezone myTZ;

    AsyncWebServer *server;
    DNSServer *dns;
    AsyncWiFiManager *wifiManager;

#ifdef ALEXA
    AlexaCallbackType alexaCallbackFn;
#endif

    void initWiFi()
    {
        wifiManager->setConfigPortalTimeout(120);
        wifiManager->setConnectTimeout(30);
        wifiManager->setDebugOutput(true);
        wifiManager->setAPStaticIPConfig(
            IPAddress(192, 168, 4, 1),
            IPAddress(192, 168, 4, 1),
            IPAddress(255, 255, 255, 0));

        wifiManager->autoConnect(TERMINAL_NAME);
        systemState.isConnected = WiFi.isConnected();
    }

    void initNTP()
    {
        waitForSync();
        myTZ.setLocation("America/Sao_Paulo");
        systemState.startedISODateTime = getISOTime();
    }

#ifdef ALEXA
    void handleAlexaCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
    {
        if (alexaCallbackFn)
        {
            alexaCallbackFn(device_id, device_name, state, value);
        }
        Logger::log(LogLevel::INFO,
                    "Alexa Device Callback: ID: %d, Name: %s, State: %d, Value: %d",
                    device_id, device_name, state, value);
    }
#endif

public:
#ifdef ALEXA
    void setCallback(AlexaCallbackType cb = nullptr)
    {
        alexaCallbackFn = cb;
    }
#endif

    WiFiConn(AsyncWebServer *srv, DNSServer *dnsServer) : server(srv), dns(dnsServer)
    {
        wifiManager = new AsyncWiFiManager(server, dns);
    }

    ~WiFiConn()
    {
        delete wifiManager;
    }

    bool begin()
    {
        initWiFi();

        if (systemState.isConnected)
        {
            initNTP();
            Logger::log(LogLevel::INFO, "WiFi connected: %s", WiFi.SSID().c_str());
            systemState.isRunning = true;
            systemState.isConnected = true;
            systemState.isInitialized = true;
        }
        else
        {
            Logger::log(LogLevel::ERROR, "Failed to connect to WiFi");
            systemState.isRunning = false;
            systemState.isConnected = false;
            systemState.isInitialized = false;
        }

        initAlexa();
        delay(500); // Stabilization after connection

#ifdef WS
        htmlServer.initWebServer(server);
        htmlServer.begin();
#else
        server->begin();
#endif

        WSLogger::initWs(*server);

        return systemState.isConnected;
    }

    void initAlexa()
    {
#ifdef ALEXA
        alexaCom.setup(server, [this](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                       { this->handleAlexaCallback(device_id, device_name, state, value); });
#endif
    }

    void loop()
    {
#ifdef ALEXA
        alexaCom.loop();
#endif

#ifdef WS
        htmlServer.process();
#endif
    }

    String getISOTime(String format = "")
    {
        const char *baseTime = "1970-01-01T00:00:00Z";

        if (WiFi.isConnected())
        {
            if (format.isEmpty())
            {
                return myTZ.dateTime(ISO8601);
            }
            return myTZ.dateTime(format);
        }
        return String(baseTime);
    }
};

// Add this near the end of WiFiConn.h, before the #endif
extern WiFiConn wifiConn;
#endif // WIFI