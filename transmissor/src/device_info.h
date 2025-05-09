#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <Arduino.h>
#include <map>

class DeviceInfo
{
public:
    static void updateDeviceList(uint8_t deviceId, const String &message); // Make updateDeviceList static
};
// namespace DeviceInfo
static std::map<uint8_t, String> deviceList; // Declare as static

#endif
