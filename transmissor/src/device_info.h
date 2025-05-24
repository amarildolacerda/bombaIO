#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <Arduino.h>
#include <vector>
#include "config.h"

#ifndef __AVR__
// Estrutura para armazenar informações do dispositivo
struct DeviceInfoData
{
    uint8_t tid;
    String event;
    String value;
    String name;
    String lastSeenISOTime;
    String status;
    int rssi;
    String uniqueName()
    {
       // char *a = ESP.getChipId();
        return name;
        // String a = name + "." + String(tid);
        // a.toLowerCase();
        // return a;
    }
    // Adicione outros campos conforme necessário
};
struct DeviceRegData
{
    uint8_t tid;
    String name;
    String toString()
    {
        name.toLowerCase();
        return String(tid) + ":" + name;
    }
    String uniqueName()
    {

        return name;
        // String a = name + "." + String(tid);
        // a.toLowerCase();
        // return a;
    }

    bool fromString(String str)
    {
        int sepIndex = str.indexOf(':');
        if (sepIndex != -1)
        {
            tid = str.substring(0, sepIndex).toInt();
            name = str.substring(sepIndex + 1);
            name.toLowerCase();
            return true;
        }
        return false;
    }
};

struct DeviceInfoHistory
{
    uint8_t tid;
    String name;
    String value;
};

#endif
class DeviceInfo
{
public:
    static void updateDeviceList(uint8_t deviceId, DeviceInfoData data);
    static void updateRegList(u_int8_t tid, DeviceRegData data);
    static String getISOTime();
    static int getTimeDifferenceSeconds(const String &lastSeenISOTime);

    static int16_t indexOf(uint8_t tid)
    {

        for (uint8_t i = 0; i < deviceRegList.size(); i++)
        {
            if (deviceRegList[i].tid == tid)
                return i;
        }

        return -1;
    }
    static String findName(uint8_t tid)
    {
        uint8_t x = indexOf(tid);
        if (x < 0)
            return "";
        deviceRegList[x].name.toLowerCase();
        return deviceRegList[x].name;
    }
    static int16_t indexOfName(String name)
    {
        name.toLowerCase();
        for (uint8_t i = 0; i < deviceRegList.size(); i++)
        {
            if (deviceRegList[i].name == name)
                return i;
        }

        return -1;
    }

    static int16_t dataOf(uint8_t tid)
    {

#ifndef __AVR__
        for (uint8_t i = 0; i < deviceList.size(); i++)
        {
            if (deviceList[i].tid == tid)
                return i;
        }

#endif
        return -1;
    }

    // Declaração correta de deviceList como membro estático
    static std::vector<DeviceInfoData> deviceList;
    static std::vector<DeviceRegData> deviceRegList;
    static std::vector<DeviceInfoHistory> history;
};

#endif
