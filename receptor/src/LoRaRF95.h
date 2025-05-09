// #ifdef LORARF95_H
// #define LORARF95_H
#include <RH_RF95.h>
#include "logger.h"

template <typename T>
class LoraRF95 : public RH_RF95<T>
{
private:
    RH_RF95<T> rf95;
    uint8_t _rxPin;
    uint8_t _txPin;
    uint8_t _irqPin;
    void (*_onReceive)(int);

public:
    void setPins(uint8_t rxPin, uint8_t txPin, uint8_t irqPin)
    {
        _rxPin = rxPin;
        _txPin = txPin;
        _irqPin = irqPin;
        pinMode(_rxPin, INPUT);
        pinMode(_txPin, OUTPUT);
    }
    void setTxPower(int level, )
    {
        rf95.setTxPower(level, false);
    }
    void begin(int band)
    {
        rf95.init();
        rf95.setFrequency(band);
    }
    bool parsePacket()
    {
        return rf95.available();
    }
    int read()
    {
        uint8_t buf;
        uint8_t len = 1;
        if (!rf95.recv(buf, len))
        {
            -1;
        }
        else
        {
            return buf;
        }
    }
    int packetRssi()
    {
        return rf95.lastRssi();
    }

    bool print(const char *message)
    {
        return rf95.send((uint8_t *)message, strlen(message));
    }
    uint8_t endPacket()
    {
        return rf95.waitPacketSent();
    }
    void onReceive(void (*callback)(int))
    {
        _onReceive = callback;

        if (callback)
        {
            pinMode(_irqPin, INPUT);
#ifdef SPI_HAS_NOTUSINGINTERRUPT
            SPI.usingInterrupt(digitalPinToInterrupt(_irqPin));
#endif
            attachInterrupt(digitalPinToInterrupt(_irqPin), LoRaClass::onDio0Rise, RISING);
        }
        else
        {
            detachInterrupt(digitalPinToInterrupt(_irqPin));
#ifdef SPI_HAS_NOTUSINGINTERRUPT
            SPI.notUsingInterrupt(digitalPinToInterrupt(_irqPin));
#endif
        }
    }
    void handle()
    {
        if (rf95.available())
        {
            if (_onReceive)
            {
                _onReceive(1);
            }
        }
    }
};

// #endif