
#ifndef ALEXACOM_H
#define ALEXACOM_H

#include <ESPAsyncWebServer.h>

struct AlexaDeviceMap
{
    uint8_t tid;     // ID do dispositivo LoRa
    uint8_t alexaId; // ID atribuído pela Espalexa
    String name;     // Nome do dispositivo
    String uniqueName()
    {
        if (name.indexOf(".") < 0)
            return name + "." + String(tid);

        return name;
    }
};

typedef std::function<void(unsigned char, const char *, bool, unsigned char)> AlexaCallbackType;

namespace AlexaCom
{
    static std::vector<AlexaDeviceMap> alexaDevices;
    static AlexaCallbackType alexaDeviceCallback;

    void setup(AsyncWebServer *server, AlexaCallbackType callback);

    void loop();
    void aliveOffLineAlexa();
    void updateStateAlexa(uint8_t tid, String uniqueName, String value);

    void addDevice(uint8_t tid, const char *name);
}

#endif