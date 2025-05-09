#ifndef LORA_INTERFACE_H
#define LORA_INTERFACE_H

#include <Arduino.h>

//[stable]
class LoRaInterface
{
protected:
    uint8_t nHeaderId = 0;
    void (*onReceiveCallBack)(int packetSize) = nullptr;

public:
    // [setup]
    virtual void setPins(int cs, int reset, int irq) = 0;
    virtual bool beginSetup(float frequency, bool promiscuous = true) = 0;
    virtual void endSetup() = 0;

    // [transmit]
    virtual bool sendMessage(uint8_t tidTo, const char *message) = 0;
    virtual bool print(const char *message) = 0; // Added print method to the interface
    uint8_t genHeaderId()
    {
        if (nHeaderId >= 255)
            nHeaderId = 0;
        return nHeaderId++;
    }
    // [receive]
    virtual bool receiveMessage(char *buffer, uint8_t &len) = 0;
    virtual int packetRssi() = 0;
    virtual int packetSnr() = 0;
    void onReceive(void (*callback)(int packetSize))
    {
        onReceiveCallBack = callback;
    }
    virtual bool available() = 0;
    virtual byte read() = 0;

    // [config]
    virtual void setHeaderTo(uint8_t tid) = 0;
    virtual void setHeaderFrom(uint8_t tid) = 0;
    // [loop]
    virtual void handle() = 0;

    virtual ~LoRaInterface() {}
};

#endif // LORA_INTERFACE_H
