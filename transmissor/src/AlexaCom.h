
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
        return name;
        // String a = name + "." + String(tid);
        // a.toLowerCase();
        // return a;
    }
};

typedef std::function<void(unsigned char, const char *, bool, unsigned char)> AlexaCallbackType;
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
    void updateStateAlexa(String uniqueName, String value);

    void addDevice(uint8_t tid, const char *name);
};

extern AlexaCom alexaCom;

#endif