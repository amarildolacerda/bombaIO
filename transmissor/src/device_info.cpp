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
#ifndef __AVR__
    // Update the device list with the event and value pair
    deviceList[deviceId] = std::make_pair(event, value);
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
