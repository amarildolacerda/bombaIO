#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <Arduino.h>

#ifndef __AVR__
#include <map>
#include <utility> // For std::pair
#endif

class DeviceInfo
{
public:
    static void updateDeviceList(uint8_t deviceId, const String &message);
    static String getISOTime();
};

#ifndef __AVR__
// Use a map to store device information as a pair of event and value
static std::map<uint8_t, std::pair<String, String>> deviceList;
#endif

#endif
