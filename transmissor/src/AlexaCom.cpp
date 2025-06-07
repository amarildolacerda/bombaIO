

#include "AlexaCom.h"
#include "fauxmoESP.h"
#include "logger.h"
#include "device_info.h"
#include "system_state.h"

fauxmoESP alexa;

AlexaCom alexaCom;

void AlexaCom::aliveOffLineAlexa()
{
    DeviceInfoData data;
    for (auto &dev : alexaDevices)
    {
        int idx = DeviceInfo::dataOf(dev.tid);
        int secs = 60;
        if (idx >= 0)
        {
            data = DeviceInfo::deviceList[idx];
            secs = DeviceInfo::getTimeDifferenceSeconds(data.lastSeen);
            if (secs >= 60 * 5)
            {
                alexa.setState(dev.uniqueName().c_str(), false, 0);
            }
            else
            {
                alexa.setState(dev.uniqueName().c_str(), data.value == "on", 100);
            }
        }
    }
}
void AlexaCom::DoCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
    if (alexaDeviceCallback)
        alexaDeviceCallback(device_id, device_name, state, value);
}
void AlexaCom::DoGetCallback(unsigned char device_id, const char *device_name)
{
    if (onGetCallbackFn)
    {
        onGetCallbackFn(device_name);
    }
}

void AlexaCom::setup(AsyncWebServer *server, AlexaCallbackType callback)
{
    alexaDeviceCallback = callback;
    Logger::info("Alexa Init");

    alexa.createServer((server == NULL) ? true : false);
    alexa.setPort(80);

    for (int i = 0; i < DeviceInfo::deviceRegList.size(); i++)
    {
        DeviceRegData reg = DeviceInfo::deviceRegList[i];
        if (reg.tid == 0)
            continue;

        addDevice(reg.tid, reg.name.c_str());
    }

    alexa.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                     { alexaCom.DoCallback(device_id, device_name, state, value); });
    alexa.onGetState([](unsigned char device_id, const char *device_name)
                     { alexaCom.DoGetCallback(device_id, device_name); });

#ifdef WS
    // These two callbacks are required for gen1 and gen3 compatibility
    server->onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                          {
                              // Serial.print("onRequestBody");
                              // Serial.println(request->url());
                              if (alexa.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data)))
                                  return;
                              // Handle any other body request here...
                          });
    server->onNotFound([](AsyncWebServerRequest *request)
                       {
                           // Serial.print("onNotFound");
                           // Serial.println(request->url());
                           String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
                           if (alexa.process(request->client(), request->method() == HTTP_GET, request->url(), body))
                               return;
                           // Handle not found request here...
                       });
#endif

    /* for (auto dev : alexaDevices)
     {
         if (dev!=NULL)
             Logger::warn(String("Reg Alexa(" + String(dev.alexaId) + "): " + String(dev.tid) + " Name: " + dev.uniqueName()).c_str());
     }
             */
    alexa.enable(true);
    Logger::log(LogLevel::INFO, "Alexa Enable(true)");
}

void AlexaCom::loop()

{
    alexa.handle();
}

void AlexaCom::updateStateAlexa(String uniqueName, String value)
{
    if (uniqueName.length() == 0)
        return; // Add validation

    uniqueName.toLowerCase();
    int idx = alexa.getDeviceId(uniqueName.c_str());
    if (idx < 0)
    {
        Logger::error(String("Nao achei alexa device: " + String(uniqueName)).c_str());
        return;
    }

    // Add null check for alexa instance
    if (&alexa)
    {
        alexa.setState(idx, value == "on", value == "on");
        Logger::info("Send to Alexa %d %s", idx, value);
    }
}

void AlexaCom::addDevice(uint8_t tid, const char *name)
{
    AlexaDeviceMap map;
    map.tid = tid;
    map.name = name;
    String aname = map.uniqueName();
    if (alexa.getDeviceId(aname.c_str()) < 0)
    {
        alexa.addDevice(aname.c_str());
        map.alexaId = alexa.getDeviceId(aname.c_str());
        alexaDevices.push_back(map);
    }
}
