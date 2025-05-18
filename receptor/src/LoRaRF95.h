#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial SSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial
#elif ESP8266
#define COMSerial Serial
#endif

class LoRaRF95
{
public:
    LoRaRF95() : rf95(COMSerial) {}
    bool initialize(float frequency, uint8_t terminalId, bool promiscuous = true)

    {
#ifdef ESP8266
        Serial.swap();
#endif
        if (!rf95.init())
        {
            Logger::log(LogLevel::ERROR, "LoRa initialization failed!");
            COMSerial.end();
            Serial.begin(115200);

            return false;
        }
        COMSerial.setTimeout(0);
        rf95.setFrequency(frequency);
        rf95.setPromiscuous(promiscuous);
        rf95.setHeaderTo(0xFF);
        rf95.setHeaderFrom(terminalId);
        rf95.setTxPower(14);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        Logger::log(LogLevel::INFO, "LoRa Server Ready");
        return true;
    }

    void sendMessage(uint8_t tid, const char *message)
    {

        rf95.setModeTx();
        rf95.setHeaderTo(tid);
        rf95.setHeaderId(genHeaderId());
        rf95.setHeaderFlags(strlen(message), 0xFF);
        rf95.send((uint8_t *)message, strlen(message));
        if (!rf95.waitPacketSent())
        {
            Logger::log(LogLevel::ERROR, "Failed to send message");
        }
        else
        {
            Logger::log(LogLevel::SEND, message);
        }
        rf95.setModeRx();
        delay(10);
    }

    bool available()
    {
        return rf95.available();
    }
    bool receiveMessage(char *buffer, uint8_t &len)
    {
        if (rf95.available())
        {
            if (rf95.recv((uint8_t *)buffer, &len))
            {
                buffer[len] = '\0';
                char msg[64];
                snprintf(msg, 64, "From: %d To: %d id: %d Flag: %d bytes: %d",
                         rf95.headerFrom(),
                         rf95.headerTo(), rf95.headerId(), len, strlen(buffer));
                Logger::log(LogLevel::RECEIVE, msg);
                Logger::log(LogLevel::RECEIVE, (char *)buffer);
                return true;
            }
        }
        return false;
    }

    int getLastRssi()
    {
        return rf95.lastRssi();
    }

private:
    RH_RF95<decltype(COMSerial)> rf95;
    uint8_t nHeaderId = 0;

    uint8_t genHeaderId()
    {
        if (nHeaderId >= 255)
            nHeaderId = 0;
        return nHeaderId++;
    }
};

// Global instance of LoRaRF95
LoRaRF95 lora;

#endif