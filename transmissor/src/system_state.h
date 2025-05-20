#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <logger.h>

class SystemState
{
private:
    bool wifiConnected = false;
    bool loraInitialized = false;
    uint8_t _tid = 0;
    std::function<void(bool)> discoveryCallback;

protected:
public:
    String relayState = "...";
    String lastUpdateTime = "";
    bool discoveryMode = false;
    unsigned long discoveryEndTime = 0;
    void registerDiscoveryCallback(std::function<void(bool)> callback)
    {
        discoveryCallback = callback;
    }
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
    void setDiscovery(bool enabled)
    {
        discoveryMode = enabled;
        discoveryEndTime = millis() + (60000 * 3); // 3 minutos
        const String msg = "Discovery mode " + (discoveryMode) ? "habilitado" : "desabilitado";
        Logger::info(msg.c_str());
        if (discoveryCallback != nullptr)
            discoveryCallback(enabled);
    }
    bool isDiscovering()
    {
        return discoveryMode;
    }

    void handle();
};

extern SystemState systemState;
#endif // SYSTEM_STATE_H