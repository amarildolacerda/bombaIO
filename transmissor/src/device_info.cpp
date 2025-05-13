#include "device_info.h"
#include <ArduinoJson.h>

// Define o membro estático corretamente
std::map<uint8_t, std::pair<String, DeviceInfoData>> DeviceInfo::deviceList;
std::map<uint8_t, std::pair<String, DeviceRegData>> DeviceInfo::deviceRegList;

void DeviceInfo::updateDeviceList(uint8_t deviceId, DeviceInfoData data)
{
#ifndef __AVR__
    if ((deviceId == 0) || (deviceId == 0xFF))
        return;
    // Atualiza a lista de dispositivos com um par de String e DeviceInfoData
    if (deviceList.find(deviceId) == deviceList.end())
    {
        data.status = "DESCONHECIDO";
        deviceList[deviceId] = std::make_pair(data.event, data);
    }
    else
    {
        // atualiza os dados
        deviceList[deviceId].first = data.event;
        deviceList[deviceId].second.event = data.event;
        deviceList[deviceId].second.value = data.value;
        deviceList[deviceId].second.rssi = data.rssi;
        deviceList[deviceId].second.lastSeenISOTime = data.lastSeenISOTime;
        if (data.event == "status")
        {
            data.value.toUpperCase();
            deviceList[deviceId].second.status = data.value;
        }
    }

#endif
}

void DeviceInfo::updateRegList(u_int8_t tid, DeviceRegData data)
{
    int n = -1;

    for (int i = 0; i < deviceRegList.size(); i++)
    {
        if (deviceRegList[i].second.tid == tid)
        {
            n = i;
            break;
        }
    }

    if (n >= 0)
        deviceRegList[n].second.name = data.name;
    else
        deviceRegList[deviceRegList.size()] = std::make_pair(data.tid, data);
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
