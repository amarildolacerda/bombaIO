#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
#endif
//--------------------------------------------------RF
SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX

class LoraRF
{
private:
    RH_RF95<decltype(RFSerial)> rf95;
    uint8_t terminalId = 1;
    bool _promiscuos = false;
    uint8_t _hto = 0xFF;
    uint8_t _hfrom = 0xFF;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';
    uint8_t sender = 0xFF;
    uint8_t nHeaderId = 0;

public:
    LoraRF() : rf95(RFSerial) {}
    bool begin(uint8_t terminal_Id, long band, bool promisc = true)
    {
        terminalId = terminal_Id;
        RFSerial.begin(Config::LORA_SPEED);
        bool result = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true); // ajusta situaçoes que não reconhece
        rf95.setFrequency(band);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        return result;
    }
    bool loop()
    {
        return true;
    }
    bool receive(char *buffer, uint8_t *len)
    {
        bool result = false;
        if (rf95.waitAvailableTimeout(100))
        {
            buffer = {0};
            result = rf95.recv((uint8_t *)buffer, len);
            if (result)
            {
                _hto = rf95.headerTo();
                _hfrom = rf95.headerFrom();
                if (_hfrom == terminalId)
                    return false;
                if ((_hto != 0xFF) && (!_promiscuos) && (_hto != terminalId))
                {
                    return false;
                }
                Logger::log(LogLevel::RECEIVE, "From: " + (String)_hfrom + " " + (String)buffer);
            }
        }
        return result;
    }
    uint8_t headerTo()
    {
        return rf95.headerTo();
    }
    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool send(uint8_t tid, String buffer)
    {
        return send(tid, (char *)buffer.c_str(), buffer.length());
    }
    bool send(uint8_t tid, char *message, uint8_t from = 0xFF, uint8_t salto = 3, uint8_t seq = 0)
    {
        bool result = false;
        uint8_t len = strlen(message);
        uint8_t fromAjustado = (from == 0xFF) ? terminalId : from;
        rf95.setModeTx();
        rf95.setHeaderFrom(fromAjustado);
        rf95.setHeaderTo(tid);
        if (seq == 0)
            seq = ++nHeaderId;
        rf95.setHeaderId(seq);
        rf95.setHeaderFlags(salto, 0xFF);

        char fullMessage[Config::MESSAGE_MAX_LEN + 3];
        fullMessage[0] = terminalId;
        fullMessage[1] = STX;
        memcpy(fullMessage + 2, message, len);
        fullMessage[len + 2] = ETX;
        fullMessage[len + 3] = '\0';

        if (rf95.send((uint8_t *)fullMessage, strlen(fullMessage)))
        {
            if (rf95.waitPacketSent(2000))
            {
#ifdef _DEBUG_ON
                printHex(fullMessage, len + 2);
#endif

                char fullLogMsg[64];

                snprintf(fullLogMsg, sizeof(fullLogMsg),
                         "[%d-%d] L:%d",
                         fromAjustado,
                         tid,
                         len);
                Logger::log(LogLevel::SEND, fullLogMsg);
                // Logger::log(LogLevel::SEND, fullMessage);
            }
        };
        rf95.setModeRx();

        return result;
    }

    void printHex(char *fullMessage, uint8_t len)
    {

        Serial.println(fullMessage);
        Serial.print("HEX: ");
        Serial.print(len);
        Serial.print(" bytes: ");
        for (size_t i = 0; i < strlen(fullMessage); i++)
        {
            if (fullMessage[i] < 0x10)
                Serial.print('0'); // zero padding para valores < 0x10
            Serial.print(fullMessage[i], HEX);
            Serial.print(' ');
        }
        Serial.println("");
    }
    bool available()
    {
        return rf95.available();
    }
};
static LoraRF lora;

#endif