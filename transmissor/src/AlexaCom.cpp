

#include "AlexaCom.h"
#include "fauxmoESP.h"
#include "logger.h"
#include "device_info.h"
#include "system_state.h"

fauxmoESP alexa;

AlexaCom alexaCom;

void AlexaCom::aliveOffLineAlexa()
{

    for (auto &dev : alexaDevices)
    {
        int idx = DeviceInfo::dataOf(dev.tid);
        int secs = 60;
        if (idx >= 0)
        {
            DeviceInfoData &data = DeviceInfo::deviceList[idx];
            secs = DeviceInfo::getTimeDifferenceSeconds(data.lastSeenISOTime);
        }
        if (secs >= 60)
        {
            alexa.setState(dev.uniqueName().c_str(), false, 0);
            Logger::warn(String(dev.name + " esta a mais de " + String(secs) + "s sem conexao ").c_str());
            //}
        }
    }
}
void AlexaCom::DoCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
    alexaDeviceCallback(device_id, device_name, state, value);
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

        // alexa.addDevice(reg.uniqueName().c_str());
        addDevice(reg.tid, reg.name.c_str());
    }

    alexa.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                     { alexaCom.DoCallback(device_id, device_name, state, value); });

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
    Serial.println(F("Alexa Enable(true)"));
}

void AlexaCom::loop()

{
    alexa.handle();
}

void AlexaCom::updateStateAlexa(uint8_t tid, String uniqueName, String value)
{
    uint8_t ct = 0;
    // nt(uniqueName);
    for (auto &dev : AlexaCom::alexaDevices)
    {
        if (dev.uniqueName().equals(uniqueName))
        {
            ct++;
            alexa.setState(dev.uniqueName().c_str(), value == "on", value == "on");
            char msg[64];
            snprintf(msg, sizeof(msg), "Term: % d Alexa %s (%d): %s (%s)", tid, dev.uniqueName(), dev.alexaId, value.c_str(), value == "on" ? "true" : "false");
            Logger::info(msg);
            return;
        }
        if (ct == 0)
        {
            Logger::error(String("nao achei alexa device: " + String(uniqueName)).c_str());
        }
    }
}

void AlexaCom::addDevice(uint8_t tid, const char *name)
{
    AlexaDeviceMap map;
    map.tid = tid;
    map.name = name;
    String aname = map.uniqueName();
    alexa.addDevice(aname.c_str());
    map.alexaId = alexa.getDeviceId(aname.c_str());
    alexaDevices.push_back(map);
}
