#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>

class SystemState
{
private:
    bool wifiConnected = false;
    bool loraInitialized = false;
    uint8_t _tid = 0;

protected:
public:
    String relayState = "...";
    String lastUpdateTime = "";
    void loraRcv(const String &message);
    void updateState(const String &newState);
    void setWifiStatus(bool connected);
    void setLoraStatus(bool initialized)
    {
        loraInitialized = initialized;
    }
    void relayStatus(const String st)
    {
        relayState = st;
    }
    String getState();
    bool isWifiConnected();
    bool isLoraInitialized();
    bool isStateValid();
    void resetStateValid();

    void handle();
};

extern SystemState systemState;
#endif // SYSTEM_STATE_H