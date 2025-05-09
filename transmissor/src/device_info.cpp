#include "device_info.h"
#include <ArduinoJson.h>

// Define the static member

void DeviceInfo::updateDeviceList(uint8_t deviceId, const String &message)
{
    // Parse the JSON message
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.println("Failed to parse JSON message");
        return;
    }

    // Extract "event" and "value" from the JSON
    String event = doc["event"].as<String>();
    String value = doc["value"].as<String>();

    // Update the device list with the event and value pair
    deviceList[deviceId] = std::make_pair(event, value);
}
