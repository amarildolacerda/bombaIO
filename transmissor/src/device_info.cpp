#include "device_info.h"
#include <ArduinoJson.h>

// Define o membro estático corretamente
std::map<uint8_t, std::pair<String, DeviceInfoData>> DeviceInfo::deviceList;

void DeviceInfo::updateDeviceList(uint8_t deviceId, const DeviceInfoData data)
{
#ifndef __AVR__
    // Atualiza a lista de dispositivos com um par de String e DeviceInfoData
    deviceList[deviceId] = std::make_pair(data.event, data);
#endif
}

String DeviceInfo::getISOTime()
{
#ifdef __AVR__
    return "1970-01-01T00:00:00Z"; // Fallback for AVR
#else

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        // Logger::log(LogLevel::WARNING, "Failed to get NTP time");
        return "1970-01-01T00:00:00Z";
    }

    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(timeStr);
#endif
}
