
#pragma once

class SystemState
{
private:
    long discoveryUpdate = 0;

public:
    uint8_t terminalId = 0;         // ID do terminal, 0 para gateway
    String terminalName = "";       // Nome do terminal, usado para identificação
    bool isGateway = true;          // tipos de estacao
    long previousMillis = 0;        // used to store the last time the system was updated
    String startedISODateTime = ""; // used to store the system start date
    bool isRunning = false;         // inLoop
    bool isConnected = false;       // wifi ok
    bool isInitialized = false;     // lora ok.
    bool isRelayOn = false;         // on/off
    bool isDiscovering = false;     // used to indicate if the system is in discovery mode

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