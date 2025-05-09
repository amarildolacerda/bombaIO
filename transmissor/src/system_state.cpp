#include "system_state.h"
#include "logger.h"
#include <time.h>
#include "config.h"
#include "display_manager.h"

SystemState systemState; // Define the global systemState object

void SystemState::loraRcv(const String &message)
{
    if (message.length() < sizeof(loraRcvMessage))
    {
        strncpy(loraRcvMessage, message.c_str(), sizeof(loraRcvMessage) - 1);
        loraRcvMessage[sizeof(loraRcvMessage) - 1] = '\0';
    }
    Logger::log(LogLevel::DEBUG, String("RCV: " + message).c_str());
}

void SystemState::resetDisplayUpdate()
{
    lastDisplayUpdate = millis();
}

void SystemState::updateState(const String &newState)
{
    if (relayState != newState)
    {
        relayState = (newState == "on" ? "LIGADO" : "DESLIGADO");
        lastUpdate = millis();
        DisplayManager::updateDisplay();
    }
}

void SystemState::setWifiStatus(bool connected)
{
    wifiConnected = connected;
    DisplayManager::updateDisplay();
}

void SystemState::setLoraStatus(bool initialized)
{
    loraInitialized = initialized;
    DisplayManager::updateDisplay();
}

void SystemState::setLoraEvent(const String &event, const String &value)
{
    loraRcvEvent = event;
    loraRcvValue = value;
    DisplayManager::updateDisplay();
}

String SystemState::getState() const
{
    return relayState;
}

bool SystemState::isWifiConnected() const
{
    return wifiConnected;
}

bool SystemState::isLoraInitialized() const
{
    return loraInitialized;
}

bool SystemState::isStateValid() const
{
    return (millis() - lastUpdate) < 300000; // 5 minutes timeout
}

String SystemState::getISOTime()
{
#ifdef __AVR__
    return "1970-01-01T00:00:00Z"; // Fallback for AVR
#else

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Logger::log(LogLevel::WARNING, "Failed to get NTP time");
        return "1970-01-01T00:00:00Z";
    }

    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(timeStr);
#endif
}

void SystemState::conditionalUpdateDisplay()
{
    if (millis() - lastDisplayUpdate >= 1000)
    { // Update every second
        DisplayManager::updateDisplay();
        lastDisplayUpdate = millis();
    }
}
