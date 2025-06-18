#pragma once

#ifdef GATEWAY

#include "Arduino.h"
#include <vector>
#include "AlexaCom.h"

struct DeviceData
{
    uint8_t tid;
    String name = "";
    bool state = 0;
    unsigned long lastSeen = 0;
    int rssi = 0;
    bool isEmpty()
    {
        return name.length() == 0;
    }
    String terminalName()
    {
        return String(tid);
    }
};

class DeviceInfo
{
private:
    std::vector<DeviceData> list;

public:
    int diffSeconds(const unsigned long &lastSeenISOTime)
    {
        return (millis() - lastSeenISOTime) / 1000; // Convert milliseconds to seconds
    }
    uint8_t running()
    {
        uint8_t n = 0;
        for (const auto &device : list)
        {
            if (diffSeconds(device.lastSeen) < 60 * 5)
            {
                n++;
            }
        }
        return n;
    }
    uint8_t size()
    {
        return list.size();
    }
    void clear()
    {
        list.clear();
    }
    void addDevice(const DeviceData &device)
    {
        for (auto &d : list)
        {
            if (d.tid == device.tid)
            {
                d.name = device.name;
                d.state = device.state;
                d.lastSeen = millis();
                d.rssi = 0;
                return;
            }
        }
        list.push_back(device);
    }
    void updateState(uint8_t tid, bool state)
    {
        for (auto &device : list)
        {
            if (device.tid == tid)
            {
                device.state = state;
                device.lastSeen = millis();
                return;
            }
        }
        DeviceData newDevice; // = {tid, "", state, millis(), 0};
        newDevice.tid = tid;
        newDevice.state = state;
        newDevice.lastSeen = millis();
        newDevice.rssi = 0;                    // Default RSSI value
        newDevice.name = "Novo" + String(tid); // Default name
        list.push_back(newDevice);
    }

    void updateDevice(uint8_t tid, const String &name, bool state, int rssi)
    {
        for (auto &device : list)
        {
            if (device.tid == tid)
            {
                if (name.length() > 0)
                {
                    device.name = name;
                }
                device.state = state;
                device.lastSeen = millis();
                device.rssi = rssi;
                return;
            }
        }
        DeviceData newDevice; // = {tid, name, state, millis(), rssi};
        newDevice.tid = tid;
        newDevice.name = name;
        newDevice.state = state;
        newDevice.lastSeen = millis();
        newDevice.rssi = rssi;
        list.push_back(newDevice);
    }
    DeviceData getDevice(uint8_t tid)
    {
        for (const auto &device : list)
        {
            if (device.tid == tid)
            {
                return device;
            }
        }
        return DeviceData(); // Return an empty DeviceData if not found
    }
    std::vector<DeviceData> getDevices()
    {
        return list;
    }
    void deleteDevice(uint8_t tid)
    {
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [tid](const DeviceData &device)
                                  { return device.tid == tid; }),
                   list.end());
    }
    int indexOf(uint8_t tid)
    {
        for (size_t i = 0; i < list.size(); i++)
        {
            if (list[i].tid == tid)
            {
                return i;
            }
        }
        return -1; // Return -1 if not found
    }
    void updateDeviceName(const uint8_t tid, String name)
    {
        int x = indexOf(tid);
        if (x >= 0)
        {
            list[x].name = name;
#ifdef ALEXA
            if (tid != 0xFF)
                alexaCom.renameDevice(tid, name);
#endif
        }
    }
};

extern DeviceInfo deviceInfo;
#endif