#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#ifdef TTGO
#include "config.h"
#include "logger.h"

#include <Adafruit_SSD1306.h>
#include <LoRa.h>
#include <WiFi.h>
#include "system_state.h"

extern Adafruit_SSD1306 display;

class DisplayManager
{
private:
    String loraRcvEvent;
    String loraRcvValue;
    uint8_t _tid;
    uint32_t lastUpdate = 0;
    bool wifiConnected;
    bool loraInitialized;
    String relayState;
    String loraRcvMessage;
    String ver = "1.0.0";
    int dispCount = 0;
    void showFooter();
public:
    int rssi = 0;
    float snr = 0;

    void initializeDisplay();
    void updateDisplay();
    void clearDisplay();
    void wifiStatus(const bool conn)
    {
        wifiConnected = conn;
    }
    void loraStatus(const bool conn)
    {
        loraInitialized = conn;
    }
    void relayStatus(const String st)
    {
        relayState = st;
    }
    void message(const String &event);
    void handle();
    void setEvent(const uint8_t id, const String event, const String value)
    {
        loraRcvEvent = event;
        loraRcvValue = value;
        _tid = id;
    }
    void resetDisplayUpdate()
    {
        lastUpdate = millis();
    }
    void setVersion(String v)
    {
        ver = v;
    }
    void setDispCount(int cnt)
    {
        dispCount = cnt;
    }
};

extern DisplayManager displayManager;

#endif
#endif // DISPLAY_MANAGER_H