#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
SoftwareSerial SSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial
#elif ESP8266 || ESP32
#define COMSerial Serial
#endif

class LoRaRF95
{
private:
    bool _promiscuos : 1;
    uint8_t terminalId = 0;
    uint8_t _retryCount = 0;
    const uint8_t MAX_RETRIES = 3;

public:
    LoRaRF95() : rf95(COMSerial) {}

    void stop()
    {
        rf95.sleep();
        COMSerial.end();
        delay(100);
    }
    void start()
    {
        COMSerial.begin(Config::LORA_SPEED);
        rf95.setModeIdle();
        rf95.setModeRx();
    }
    bool reactive()
    {
        stop();
        start();
        return true;
    }

    bool initialize(float frequency, uint8_t terminal_Id, bool promiscuous = true)
    {
#ifdef ESP8266
        Serial.swap();
#endif
        COMSerial.begin(9600);
        if (!rf95.init())
        {
            Logger::log(LogLevel::ERROR, F("LoRa initialization failed!"));

            start();
            Serial.begin(115200);
            return false;
        }

        _promiscuos = promiscuous;
        terminalId = terminal_Id;
        COMSerial.setTimeout(0);
        rf95.setFrequency(frequency);
        rf95.setPromiscuous(true); // Usa o parâmetro fornecido
        rf95.setHeaderTo(0xFF);
        rf95.setHeaderFrom(terminalId);
        rf95.setTxPower(14, false); // Melhor controle de potência
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        return true;
    }

    bool loop()
    {
        return true;
    }
    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool sendMessage(uint8_t tid, const char *message, uint8_t from = 0xFF, uint8_t salto = 3, uint8_t seq = 0)
    {
        uint8_t len = strlen(message);
        if (len > RH_RF95_MAX_MESSAGE_LEN)
        {
            return false;
        }

        for (_retryCount = 0; _retryCount < MAX_RETRIES; _retryCount++)
        {
            rf95.setModeTx();
            rf95.setHeaderTo(tid);
            rf95.setHeaderFrom((from != 0xFF) ? from : terminalId);
            if (seq == 0)
                seq = ++nHeaderId;
            rf95.setHeaderId(seq);
            rf95.setHeaderFlags(len, 0xFF); // Flags separadas do tamanho

            if (rf95.send((uint8_t *)message, len))
            {
                if (rf95.waitPacketSent(2000))
                { // Timeout de 2s
                    Logger::log(LogLevel::SEND, message);
                    rf95.setModeRx();
                    return true;
                }
            }
            delay(100 * _retryCount); // Backoff exponencial
        }

        rf95.setModeRx();
        return false;
    }

    bool available(uint16_t timeout = 50)
    {
        return rf95.waitAvailableTimeout(timeout);
    }

    bool receiveMessage(char *buffer, uint8_t *len)
    {
        if (!rf95.waitAvailableTimeout(10))
            return false;

        uint8_t recvLen = RH_RF95_MAX_MESSAGE_LEN;
        if (rf95.recv((uint8_t *)buffer, &recvLen))
        {
            buffer[recvLen] = '\0';
            *len = recvLen;

            // Filtro de destino
            uint8_t mto = rf95.headerTo();
            uint8_t salto = rf95.headerFlags();
            uint8_t mfrom = rf95.headerFrom();
            if (mfrom == terminalId)
                return false;

            if ((mto == terminalId) || (mto = 0xFF))
            {

                char fullLogMsg[Config::MESSAGE_MAX_LEN + 64];

                snprintf(fullLogMsg, sizeof(fullLogMsg),
                         "[%d→%d] L:%d B: %s",
                         rf95.headerFrom(),
                         rf95.headerTo(),
                         recvLen,
                         buffer);

                Logger::log(LogLevel::RECEIVE, fullLogMsg);
            }
            if (salto > 1)
            {
                if (mto != terminalId)
                {
                    sendMessage(rf95.headerTo(), buffer, rf95.headerFrom(), --salto, rf95.headerId());
                }
            }
            return _promiscuos;
        }
        return false;
    }

    int getLastRssi() { return rf95.lastRssi(); }

    RH_RF95<decltype(COMSerial)> getRf95()
    {
        return rf95;
    }

private:
    RH_RF95<decltype(COMSerial)> rf95;
    uint8_t nHeaderId = 0;
};

// Global instance
LoRaRF95 lora;

#endif