#ifndef DISPLAY_INTERFACE_H
#define DISPLAY_INTERFACE_H

#include <Arduino.h>

class DisplayInterface
{
public:
    virtual ~DisplayInterface() {}

    // Métodos básicos
    virtual void initialize() = 0;
    virtual void clear() = 0;
    virtual void update() = 0;
    virtual void handle() = 0;

    // Métodos de status
    virtual void setWifiStatus(bool connected) = 0;
    virtual void setLoraStatus(bool initialized) = 0;
    virtual void setRelayStatus(const String &status) = 0;

    // Métodos de mensagens
    virtual void showMessage(const String &message) = 0;
    virtual void showInfo(const String &info) = 0;
    virtual void showEvent(uint8_t id, const String &event, const String &value) = 0;

    // Configurações
    virtual void setVersion(const String &version) = 0;
    virtual void setDisplayCount(int count) = 0;

    // Dados de rede
    int rssi = 0;
    float snr = 0.0;
};

#endif // DISPLAY_INTERFACE_H