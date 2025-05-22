
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
        String a = name + "." + String(tid);
        a.toLowerCase();
        return a;
    }
};

typedef std::function<void(unsigned char, const char *, bool, unsigned char)> AlexaCallbackType;

class AlexaCom
{
private:
    void DoCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value);

public:
    std::vector<AlexaDeviceMap> alexaDevices;
    AlexaCallbackType alexaDeviceCallback;

    void setup(AsyncWebServer *server, AlexaCallbackType callback);

    void loop();
    void aliveOffLineAlexa();
    void updateStateAlexa(uint8_t tid, String uniqueName, String value);

    void addDevice(uint8_t tid, const char *name);
};

extern AlexaCom alexaCom;

#endif