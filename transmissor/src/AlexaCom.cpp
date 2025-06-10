
#ifdef ALEXA
#include "AlexaCom.h"
#include "deviceinfo.h"
#include "systemstate.h"
#include "ESPAsyncWebServer.h"

#ifdef ALEXA
#include "fauxmoESP.h"
fauxmoESP alexa;
#endif

AlexaCom alexaCom;

void AlexaCom::aliveOffLineAlexa()
{
    DeviceData data;
    for (auto &dev : alexaDevices)
    {
        int idx = deviceInfo.indexOf(dev.tid);
        int secs = 60;
        if (idx >= 0)
        {
            data = deviceInfo.getDevices()[idx];
            secs = deviceInfo.diffSeconds(data.lastSeen);
#ifdef ALEXA
            if (secs >= 60 * 5)
            {
                alexa.setState(dev.uniqueName().c_str(), false, 0);
            }
            else
            {
                alexa.setState(dev.uniqueName().c_str(), data.state, 100);
            }
#endif
        }
    }
}

void AlexaCom::DoCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
    int idx = findBYAlexaId(device_id);
    if (idx < 0)
        return;
    if (alexaDeviceCallback)
        alexaDeviceCallback(alexaDevices[idx].tid, device_name, state, value);
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
#ifdef ALEXA
    Logger::info("Alexa Init");

    alexa.createServer((server == NULL) ? true : false);
    alexa.setPort(80);

    for (size_t i = 0; i < deviceInfo.size(); i++)
    {
        DeviceData reg = deviceInfo.getDevices()[i];
        if (reg.tid == 0)
            continue;

        AlexaDeviceMap map;
        map.tid = reg.tid;
        map.name = reg.name;

        addDevice(reg.tid, map.uniqueName().c_str());
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
#endif
}

void AlexaCom::renameDevice(const uint8_t tid, String name)
{

    size_t i = indexOf(tid);
    if (i >= 0)
    {
        String oldname = alexaDevices[i].name;
        alexaDevices[i].name = name;
        alexa.renameDevice(oldname.c_str(), name.c_str());
    }
}

void AlexaCom::loop()

{
#ifdef ALEXA
    alexa.handle();
#endif
}

void AlexaCom::updateStateAlexa(const uint8_t tid, const bool value)
{
    uint8_t id = indexOf(tid);
    if (id < 0)
        return; // Device not found
    uint8_t alexaId = alexaDevices[id].alexaId;

#ifdef ALEXA
    alexa.setState(alexaId, value, value);
    Logger::info("Send to Alexa %d %s", alexaId, value ? "on" : "off");
#endif
}

void AlexaCom::addDevice(uint8_t tid, const char *name)
{
    AlexaDeviceMap map;
    map.tid = tid;
    map.name = name;
    String aname = map.uniqueName();
#ifdef ALEXA
    if (alexa.getDeviceId(aname.c_str()) < 0)
    {
        alexa.addDevice(aname.c_str());
        map.alexaId = alexa.getDeviceId(aname.c_str());
        alexaDevices.push_back(map);
    }
    Logger::info("Adicionou Alexa: %s", aname);
#endif
}
#endif