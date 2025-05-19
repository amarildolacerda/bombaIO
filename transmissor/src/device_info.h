#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <Arduino.h>

#ifndef __AVR__
#include <map>
#include <utility> // For std::pair
#endif

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
    // Adicione outros campos conforme necessário
};
struct DeviceRegData
{
    uint8_t tid;
    String name;
    String toString()
    {
        return String(tid) + ":" + name;
    }
    bool fromString(String str)
    {
        int sepIndex = str.indexOf(':');
        if (sepIndex != -1)
        {
            tid = str.substring(0, sepIndex).toInt();
            name = str.substring(sepIndex + 1);
            return true;
        }
        return false;
    }
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

#ifndef __AVR__
        for (uint8_t i = 0; i < deviceRegList.size(); i++)
        {
            if (deviceRegList[i].second.tid == tid)
                return i;
        }

#endif
        return -1;
    }

    static int16_t dataOf(uint8_t tid)
    {

#ifndef __AVR__
        for (uint8_t i = 0; i < deviceList.size(); i++)
        {
            if (deviceList[i].second.tid == tid)
                return i;
        }

#endif
        return -1;
    }

#ifndef __AVR__
    // Declaração correta de deviceList como membro estático
    static std::map<uint8_t, std::pair<String, DeviceInfoData>> deviceList;
    static std::map<uint8_t, std::pair<String, DeviceRegData>> deviceRegList;

#endif
};

#endif
