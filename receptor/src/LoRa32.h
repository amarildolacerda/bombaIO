#ifndef LORA32_H
#define LORA32_H
#ifdef ESP32

#include "Arduino.h"
#include "queue_message.h"
#include "LoRa.h"

class LoRa32
{
private:
    FifoList txQueue;
    FifoList rxQueue;
    bool inPromiscuous = true;

public:
    bool connected = false;
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true)
    {
        LoRa.begin(band);
        LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        LoRa.setSignalBandwidth(125E3); // 125kHz
        LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setTxPower(14);
        LoRa.setPreambleLength(8);

        inPromiscuous = promisc;
        LoRa.receive();

        return true;
    }
    bool loop()
    {

        return false;
    }
    void send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId)
    {
    }
    bool processIncoming(MessageRec &rec)
    {
        return false;
    }
};

LoRa32 lora;
#endif
#endif
