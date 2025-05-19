#include "system_state.h"
#include "logger.h"
#include <time.h>
#include "config.h"
#include "display_manager.h"
#include "device_info.h"

SystemState systemState; // Define the global systemState object

void SystemState::loraRcv(const String &message)
{

    Logger::log(LogLevel::DEBUG, message.c_str());
}

long lastUpdate = 0;
void SystemState::updateState(const String &newState)
{

    lastUpdateTime = DeviceInfo::getISOTime().substring(11, 19);
    relayState = (newState == "on" ? "LIGADO" : "DESLIGADO");
}

void SystemState::setWifiStatus(bool connected)
{
    wifiConnected = connected;
}

String SystemState::getState()
{
    return relayState;
}

bool SystemState::isWifiConnected()
{
    return wifiConnected;
}

bool SystemState::isLoraInitialized()
{
    return loraInitialized;
}

bool SystemState::isStateValid()
{
    return (millis() - lastUpdate) < Config::STATE_CHECK_INTERVAL; // 5 minutes timeout
}

void SystemState::resetStateValid()
{
    lastUpdate = millis();
}

void SystemState::handle()
{
#ifdef TTGO
    displayManager.wifiStatus(wifiConnected);
    displayManager.loraStatus(loraInitialized);
    displayManager.relayStatus(relayState);
#endif
    if (discoveryMode && millis() > discoveryEndTime)
    {
        discoveryMode = false;
    }
}
