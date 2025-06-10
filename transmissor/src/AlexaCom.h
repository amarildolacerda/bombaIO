
#ifndef ALEXACOM_H
#define ALEXACOM_H

#ifdef ALEXA
#include <ESPAsyncWebServer.h>
#include "logger.h"

struct AlexaDeviceMap
{
    uint8_t tid;     // ID do dispositivo LoRa
    uint8_t alexaId; // ID atribuído pela Espalexa
    String name;     // Nome do dispositivo
    String uniqueName()
    {
        return name + String(tid);
        // String a = name + "." + String(tid);
        // a.toLowerCase();
        // return a;
    }
};

typedef std::function<void(const uint8_t tid, const char *, bool, unsigned char)> AlexaCallbackType;
typedef std::function<void(const char *)> AlexaOnGetCallback;

class AlexaCom
{
private:
    void DoCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value);
    void DoGetCallback(unsigned char device_id, const char *device_name);

public:
    std::vector<AlexaDeviceMap> alexaDevices;
    AlexaCallbackType alexaDeviceCallback;
    AlexaOnGetCallback onGetCallbackFn;

    void setup(AsyncWebServer *server, AlexaCallbackType callback);
    void onGetCallback(AlexaOnGetCallback fn)
    {
        onGetCallbackFn = fn;
    }

    void loop();
    void aliveOffLineAlexa();
    void updateStateAlexa(const uint8_t tid, const bool value);

    void addDevice(uint8_t tid, const char *name);
    int indexOf(const uint8_t tid)
    {
        for (size_t i = 0; i < alexaDevices.size(); i++)
        {
            if (alexaDevices[i].tid == tid)
                return i;
        }
        Logger::warn("Alexa, nao achei o terminal  %d", tid);
        return -1;
    }
    int findBYAlexaId(unsigned char device_id)
    {
        for (size_t i = 0; i < alexaDevices.size(); i++)
        {
            if (alexaDevices[i].alexaId == device_id)
                return i;
        }
        Logger::warn("Alexa, nao registrou o terminal %d", device_id);
        return -1;
    }
    void renameDevice(const uint8_t tid, String name);
};

extern AlexaCom alexaCom;

#endif
#endif