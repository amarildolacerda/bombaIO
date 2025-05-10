#ifndef LORARF95_H
#define LORARF95_H

#include "config.h"
#include "LoRaInterface.h"
#include <RH_RF95.h>
#include "logger.h"

#ifdef ESP8266
RH_RF95 rf95(Serial);
#elif __AVR__
#include <SoftwareSerial.h>
static SoftwareSerial SSerial(Config::LORA_RX_PIN, Config::LORA_TX_PIN);
// #define LoRaSerial SSerial

static RH_RF95<SoftwareSerial> rf95(SSerial);

#endif

class LoRaRF95 : public LoRaInterface
{
private:
public:
    bool beginSetup(float frequency, bool promiscuous = true) override
    {
        if (!rf95.init())
        {
            Logger::log(LogLevel::ERROR, "LoRa initialization failed!");
            return false;
        }
        rf95.setFrequency(frequency);
        rf95.setPromiscuous(true); // (promiscuous);
        rf95.setTxPower(14);
        Logger::log(LogLevel::INFO, "LoRa initialized successfully (RF95).");
        return true;
    }

    bool sendMessage(uint8_t tid, const char *message) override
    {

        if (!message)
        {
            Logger::error("Message is null");
            return false;
        }
        rf95.setHeaderTo(tid);
        rf95.setHeaderId(genHeaderId());
        rf95.setHeaderFrom(Config::TERMINAL_ID);

        rf95.send((uint8_t *)message, strlen(message));
        Logger::verbose(message);
        return rf95.waitPacketSent();
    }

    bool receiveMessage(char *buffer, uint8_t &len) override
    {
        if (!buffer || len > Config::MESSAGE_LEN)
            return false;
        if (rf95.available())
        {
            if (rf95.recv((uint8_t *)buffer, &len))
            {
                buffer[len] = '\0';
                return true;
            }
        }
        return false;
    }

    int packetRssi() override
    {
        return rf95.lastRssi();
    }

    void handle()
    {
        if (rf95.available()) //&& onReceiveCallBack)
        {
            onReceiveCallBack(rf95.lastRssi());
        }
    }
    void setHeaderTo(uint8_t tid) override
    {
        rf95.setHeaderTo(tid);
    }
    void setHeaderFrom(uint8_t tid) override
    {
        rf95.setHeaderFrom(tid);
    }
    void setPins(int cs, int reset, int irq) override
    {
        //   rf95.setPins(cs, reset, irq);
    }
    void endSetup() override
    {
    }
    bool print(const char *message) override
    {
        return sendMessage(0xFF, message);
    }
    bool available() override
    {
        return rf95.available();
    }
    byte read() override
    {
        char buffer[1] = {0};
        uint8_t len = sizeof(buffer);
        bool rt = receiveMessage(buffer, len);
        if (rt)
        {
            return buffer[0];
        }
        else
            return 0;
    }
    int packetSnr() override
    {
        return 0; // rf95.lastSNR(); indefined
    }
};

#endif
