#ifndef LORA_INTERFACE_H
#define LORA_INTERFACE_H

#include <Arduino.h>
#include <queue_message.h>

//[stable]
class LoRaInterface
{
public:
    virtual bool begin(const uint8_t terminal_Id, long band, bool promisc = true) = 0;
    virtual void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) {};
    virtual void setHeaderFrom(const uint8_t tid) = 0;
    virtual void endSetup()
    {
    }
    virtual int packetRssi()
    {
        return 0;
    };
    virtual int packetSnr()
    {
        return 0;
    }
    virtual bool available() = 0;

    virtual ~LoRaInterface() {}
    virtual bool send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId) = 0;
    virtual bool processIncoming(MessageRec &rec) = 0;
    virtual bool loop() = 0;
};

#endif // LORA_INTERFACE_H
