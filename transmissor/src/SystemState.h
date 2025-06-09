
#pragma once
class SystemState
{
private:
    long discoveryUpdate = 0;

public:
    long previousMillis = 0;
    String startedISODateTime = ""; // used to store the system start date
    bool isRunning = false;
    bool isConnected = false;
    bool isInitialized = false;
    bool isRelayOn = false;
    bool isDiscovering = false; // used to indicate if the system is in discovery mode

    bool setDiscovering(bool value)
    {
        isDiscovering = value;
        discoveryUpdate = millis();
        return isDiscovering;
    }
    void handle()
    {
        if (isDiscovering && millis() - discoveryUpdate > 60000)
        {
            isDiscovering = false; // stop discovery after timeout
            Logger::log(LogLevel::INFO, "Discovery timeout reached, stopping discovery.");
        }
    }
};

static SystemState systemState;