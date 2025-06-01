#ifndef TTGO_DISPLAY_H
#define TTGO_DISPLAY_H

#ifdef DISPLAYTTGO
#include "DisplayInterface.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>

extern Adafruit_SSD1306 display;

class TtgoDisplay : public DisplayInterface
{
private:
    String loraRcvEvent;
    String loraRcvValue;
    uint8_t terminalId;
    uint32_t lastUpdate = 0;
    bool wifiConnected = false;
    bool loraInitialized = false;
    String relayState;
    String version = "1.0.0";
    int displayCount = 0;
    uint32_t stateTimeout = 1000;

    void showFooter()
    {
        display.setTextSize(1);
        display.setCursor(0, 56);
        display.print("v");
        display.print(version);
        display.print(" ");
        display.print(displayCount);
    }

public:
    void initialize() override
    {
        display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
        display.clearDisplay();
        display.display();
        display.setTextSize(1);
        display.setTextColor(WHITE);
    }

    void clear() override
    {
        display.clearDisplay();
    }

    void update() override
    {
        display.clearDisplay();

        // Header
        display.setCursor(0, 0);
        display.print("TTGO LoRa v");
        display.print(version);

        // Network status
        display.setCursor(0, 10);
        display.print(wifiConnected ? "WiFi: ON" : "WiFi: OFF");
        display.setCursor(64, 10);
        display.print(loraInitialized ? "LoRa: ON" : "LoRa: OFF");

        if (loraInitialized)
        {
            display.setCursor(0, 20);
            display.print("RSSI:");
            display.print(rssi);
            display.print(" SNR:");
            display.print(snr, 1);
        }

        // Relay status
        display.setCursor(0, 30);
        display.print("Relay: ");
        display.print(relayState);

        // LoRa message
        if (millis() - lastUpdate < stateTimeout)
        {
            display.setCursor(0, 40);
            display.print("[");
            display.print(terminalId);
            display.print("] ");
            display.print(loraRcvEvent);
            if (!loraRcvValue.isEmpty())
            {
                display.print(": ");
                display.print(loraRcvValue);
            }
        }

        showFooter();
        display.display();
    }

    void handle() override
    {
        static uint32_t lastHandle = 0;
        if (millis() - lastHandle > 500)
        {
            lastHandle = millis();
            update();
        }
    }

    void setWifiStatus(bool connected) override
    {
        wifiConnected = connected;
    }

    void setLoraStatus(bool initialized) override
    {
        loraInitialized = initialized;
    }

    void setRelayStatus(const String &status) override
    {
        relayState = status;
    }

    void showMessage(const String &message) override
    {
        display.clearDisplay();
        display.setCursor(10, 20);
        display.setTextSize(2);
        display.print(message);
        display.display();
        delay(2000);
        display.setTextSize(1);
    }

    void showInfo(const String &info) override
    {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.print(info);
        display.display();
        delay(1000);
    }

    void showEvent(uint8_t id, const String &event, const String &value) override
    {
        terminalId = id;
        loraRcvEvent = event;
        loraRcvValue = value;
        lastUpdate = millis();
    }

    void setVersion(const String &v) override
    {
        version = v;
    }

    void setDisplayCount(int count) override
    {
        displayCount = count;
    }
};

#endif // TTGO
#endif // TTGO_DISPLAY_H