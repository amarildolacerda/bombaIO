#ifndef HELTEC_DISPLAY_H
#define HELTEC_DISPLAY_H

#ifdef HELTEC
#include "DisplayInterface.h"
#include "config.h"
#include "logger.h"
#include <Wire.h>
#include "heltec.h"
#define htdisplay Heltec.display
// #include "HT_SSD1306Wire.h"

// #include "OLEDDisplay->h"
//  #include "images.h"

// static SSD1306Wire htdisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

class HeltecDisplay : public DisplayInterface
{
private:
    // OLEDDisplay *htdisplay;
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

    void drawHeader()
    {
        htdisplay->setTextAlignment(TEXT_ALIGN_LEFT);
        htdisplay->setFont(ArialMT_Plain_10);
        htdisplay->drawString(0, 0, "Heltec LoRa32 v" + version);
    }

    void drawNetworkStatus()
    {
        htdisplay->setTextAlignment(TEXT_ALIGN_LEFT);
        htdisplay->setFont(ArialMT_Plain_10);

        String wifiStatus = wifiConnected ? "WiFi: ON" : "WiFi: OFF";
        String loraStatus = loraInitialized ? "LoRa: ON" : "LoRa: OFF";

        htdisplay->drawString(0, 12, wifiStatus);
        htdisplay->drawString(64, 12, loraStatus);

        if (loraInitialized)
        {
            String signalInfo = "RSSI: " + String(rssi) + " SNR: " + String(snr, 1);
            htdisplay->drawString(0, 24, signalInfo);
        }
    }

    void drawRelayStatus()
    {
        htdisplay->setTextAlignment(TEXT_ALIGN_LEFT);
        htdisplay->setFont(ArialMT_Plain_16);
        htdisplay->drawString(0, 36, "Relay: " + relayState);
    }

    void drawLoraMessage()
    {
        if (millis() - lastUpdate < stateTimeout)
        {
            htdisplay->setTextAlignment(TEXT_ALIGN_LEFT);
            htdisplay->setFont(ArialMT_Plain_10);

            String msg = "[" + String(terminalId) + "] " + loraRcvEvent;
            if (!loraRcvValue.isEmpty())
            {
                msg += ": " + loraRcvValue;
            }

            htdisplay->drawStringMaxWidth(0, 54, 128, msg);
        }
    }
    void VextON(void)
    {
        pinMode(Vext, OUTPUT);
        digitalWrite(Vext, LOW);
        delay(100);
    }

    void VextOFF(void) // Vext default OFF
    {
        pinMode(Vext, OUTPUT);
        digitalWrite(Vext, HIGH);
        delay(100);
    }

public:
    HeltecDisplay()
    {
        if (!htdisplay)
            Heltec.begin(Heltec_Screen, false, false);
        //  htdisplay = new OLEDDisplay(128, 64, &Wire, OLED_RST);
    }

    void initialize() override
    {
        VextON();
        delay(100);

        // Initialising the UI will init the htdisplay too.
        htdisplay->init();
        // Wire.begin(OLED_SDA, OLED_SCL);
        htdisplay->flipScreenVertically();
        htdisplay->setFont(ArialMT_Plain_10);
        htdisplay->clear();
        htdisplay->display();
    }

    void clear() override
    {
        htdisplay->clear();
    }

    void update() override
    {
        htdisplay->clear();
        drawHeader();
        drawNetworkStatus();
        drawRelayStatus();
        drawLoraMessage();
        htdisplay->display();
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
        htdisplay->clear();
        htdisplay->setTextAlignment(TEXT_ALIGN_CENTER);
        htdisplay->setFont(ArialMT_Plain_16);
        htdisplay->drawString(64, 20, message);
        htdisplay->display();
        delay(2000);
    }

    void showInfo(const String &info) override
    {
        htdisplay->clear();
        htdisplay->setTextAlignment(TEXT_ALIGN_LEFT);
        htdisplay->setFont(ArialMT_Plain_10);
        htdisplay->drawStringMaxWidth(0, 0, 128, info);
        htdisplay->display();
        delay(1000);
    }

    void setLoraMessage(uint8_t id, const String &event, const String &value) override
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

extern HeltecDisplay displayManager;

#endif // HELTEC
#endif // HELTEC_DISPLAY_H