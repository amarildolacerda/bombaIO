#include "device_info.h"

// Define the static member

void DeviceInfo::updateDeviceList(uint8_t deviceId, const String &message)
{
    deviceList[deviceId] = message;
}
