
#ifndef PREFERS_H
#define PREFERS_H
#ifndef ESP8266
#include "Preferences.h"
#endif
#include "device_info.h"

class Prefers
{
private:
    const String filename = "wifi-config";

public:
    void save()
    {
        //
    }
    void restore()
    {
        //
    }
    static void saveRegs()
    {
        //
        for (size_t i = 0; i < DeviceInfo::deviceRegList.size(); i++)
        {
            /* code */
            DeviceRegData data = DeviceInfo::deviceRegList[i];
            if (data.tid == 0)
                continue;
            // Serial.print("  ");
            // Serial.println(data.toString());
#ifndef ESP8266
            Preferences prefs;
            prefs.begin("devices", false);
            prefs.putString(("reg" + String(i)).c_str(), data.toString().c_str());
            prefs.end();
#endif
        }
    }

    static void restoreRegs()
    {
#ifndef ESP8266
        DeviceInfo::deviceRegList.clear();
        Preferences prefs;
        prefs.begin("devices", true);
        for (size_t i = 0;; i++)
        {
            String key = "reg" + String(i);
            if (!prefs.isKey(key.c_str()))
                break;
            String value = prefs.getString(key.c_str(), "");
            // Serial.print(value);
            if (value.length() == 0)
                break;
            DeviceRegData data;
            if (data.fromString(value))
            {
                DeviceInfo::updateRegList(data.tid, data);
            }
        }
        prefs.end();
#endif
    }
};

#endif