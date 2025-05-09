#ifndef LORARF95_H
#define LORARF95_H

#ifdef ESP8266

#elif avr
#include <SoftwareSerial.h>
SoftwareSerial SSerial(10, 11);
#define LoRaSerial SSerial
#endif

#include "config.h"

class LoRaRF95
{
private:
    uint8_t nHeaderId = 0;
    uint8_t _tid = 0xFF;
    uint8_t _tidTo = 0xFF;
    // declarar callback aqui
    void (*onReceiveCallBack)(int packetSize) = nullptr;

protected:
public:
    bool begin(float frequency, bool promiscuous = true);
    void sendMessage(uint8_t tid, const char *message);
    bool receiveMessage(char *buffer, uint8_t &len);
    int16_t packetRssi();
    uint8_t packetSnr() { return 0; }
    uint8_t genHeaderId();
    void setPins(int cs, int reset, int irq) {}
    void setSyncWord(const uint8_t tid);
    void onReceive(void (*callback)(int packetSize))
    {
        onReceiveCallBack = callback;
    }
    static void receive() {}
    void print(const char *message);
    bool beginPacket();
    int endPacket();
    bool parsePacket();
    bool available();
    byte read();
    void setHeaderTo(uint8_t tid) { _tidTo = tid; }
};

#ifdef RF95
extern LoRaRF95 LoRa;
#endif

#endif
