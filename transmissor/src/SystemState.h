
#pragma once
class SystemState
{
public:
    long previousMillis = 0;
    String startedISODateTime = ""; // used to store the system start date
    bool isRunning = false;
    bool isConnected = false;
    bool isInitialized = false;
    bool isRelayOn = false;
    bool isDiscovering = false; // used to indicate if the system is in discovery mode
};

static SystemState systemState;