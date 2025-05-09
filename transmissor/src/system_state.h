#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class SystemState
{
private:
protected:
public:
    String loraRcvEvent = "";
    String loraRcvValue = "";
    char loraRcvMessage[256] = ""; // Buffer for received messages
    String relayState = "DESCONHECIDO";
    uint32_t lastUpdate = 0;
    bool wifiConnected = false;
    bool loraInitialized = false;
    uint32_t lastDisplayUpdate = 0;

    void loraRcv(const String &message);
    void resetDisplayUpdate();
    void updateState(const String &newState);
    void setWifiStatus(bool connected);
    void setLoraStatus(bool initialized);
    void setLoraEvent(const String &event, const String &value);
    String getState() const;
    bool isWifiConnected() const;
    bool isLoraInitialized() const;
    bool isStateValid() const;
    String getISOTime();
    void conditionalUpdateDisplay();
};

extern SystemState systemState;
#endif // SYSTEM_STATE_H