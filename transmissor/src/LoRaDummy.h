#ifndef LORA_DUMMY_H
#define LORA_DUMMY_H
#include "lorainterface.h"
#include "stats.h"

class LoRaDummy : public LoRaInterface
{

public:
    bool sendMessage(MessageRec &rec)
    {
        stats.txCount++;
        Serial.println("LoRa Dummy");
#ifdef DEBUG_ON
        rec.print();
#endif
        return true;
    };
    bool receiveMessage()
    {
        stats.rxCount++;
        return true;
    };
    void modeRx() {};
    void modeTx()
    {
        setState(LoRaRX);
    };
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true)
    {
        connected = true;
        return true;
    };
};
#endif