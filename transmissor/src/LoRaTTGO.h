#ifndef LORATTGO_H
#define LORATTGO_H

#include "LoRaInterface.h"
#include <Arduino.h>
#include <LoRa.h>

class LoRaTTGO : public LoRaInterface
{
private:
    uint8_t tid = 0xFF;
    uint8_t tidTo = 0xFF;

public:
    bool beginSetup(float frequency, bool promiscuous = true) override
    {
        LoRa.begin(frequency);
        LoRa.setTxPower(14);
        LoRa.setPreambleLength(8);
        return true;
    }
    void sendHeader(uint8_t _tidTo = -1)
    {
        LoRa.write(_tidTo > -1 ? _tidTo : tidTo);
        LoRa.write(tid);
        LoRa.write(genHeaderId());
        LoRa.write(0xFF); // 0x00 = no future
    }

    bool sendMessage(uint8_t tid, const char *message) override
    {
        tidTo = tid;
        LoRa.beginPacket();
        sendHeader(tid);
        LoRa.print(message);
        return LoRa.endPacket() > 0;
    }
    bool receiveMessage(char *buffer, uint8_t &len) override
    {
        int packetSize = LoRa.parsePacket();
        if (packetSize)
        {
            len = 0;
            while (LoRa.available())
            {
                char c = (char)LoRa.read();
                buffer[len++] = c;
            }
            buffer[len] = '\0';
            return true;
        }
        return false;
    }

    bool print(const char *message) override
    {
        return sendMessage(0xFF, message);
    }
    void setHeaderTo(uint8_t tid) override
    {
        tidTo = tid;
    }
    void setHeaderFrom(uint8_t tid) override
    {
        this->tid = tid;
    }

    void setPins(int cs, int reset, int irq) override
    {
        LoRa.setPins(cs, reset, irq);
    }
    void endSetup() override
    {
    }
    void handle() override
    {
        // usa o call back
    }
    byte read() override
    {
        return LoRa.read();
    }

    int packetRssi() override
    {
        return LoRa.packetRssi();
    }
    int packetSnr() override
    {
        return LoRa.packetSnr();
    }

    bool available() override
    {
        return LoRa.available();
    }
};

#endif // LORATTGO_H
