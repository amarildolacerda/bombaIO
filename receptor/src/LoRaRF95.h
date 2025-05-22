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
private:
    bool _promiscuos : 1;
    uint8_t _tid = 0;

public:
    LoRaRF95() : rf95(COMSerial) {}
    bool reactive()
    {
        if (rf95.sleep())
            return rf95.init();
        return false;
    }
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
        _promiscuos = promiscuous;
        _tid = terminalId;
        COMSerial.setTimeout(0);
        rf95.setFrequency(frequency);
        rf95.setPromiscuous(true);
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
        delay(50);
        rf95.setModeRx();
        delay(50);
    }

    bool available()
    {
        return rf95.available();
    }
    uint8_t headerId()
    {
        return rf95.headerId();
    }
    int8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool receiveMessage(char *buffer, uint8_t &len)
    {
        if (rf95.available())
        {
            if (rf95.recv((uint8_t *)buffer, &len))
            {
                buffer[len] = '\0';
                static char msg[64];
                memset(msg, 0, sizeof(msg));
                snprintf(msg, sizeof(msg), "From: %d To: %d id: %d Flag: %d bytes: %d",
                         rf95.headerFrom(),
                         rf95.headerTo(), rf95.headerId(), rf95.headerFlags(), len);

                Logger::log(LogLevel::RECEIVE, msg);

                if (rf95.headerFlags() - len != 0)
                {
                    Logger::log(LogLevel::RECEIVE, F("Pacote inválido"));
                    return false;
                }
                Logger::log(LogLevel::RECEIVE, (char *)buffer);
                uint8_t mto = rf95.headerTo();
                if (((mto == 0xFF) || (mto == _tid)) || (_promiscuos))
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