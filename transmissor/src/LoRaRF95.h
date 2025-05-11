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
    uint8_t _tid = 0xFF;

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
        rf95.setTxPower(23, false);
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
        // Serial.print(message);
        return rf95.waitPacketSent();
    }

    bool receiveMessage(uint8_t *buffer, uint8_t &len) override
    {
        if (!buffer || len == 0 || len > Config::MESSAGE_LEN)
            return false;
        if (rf95.available())
        {
            uint8_t recvLen = len;
            if (rf95.recv(buffer, &recvLen))
            {
                // Busca o último caractere '}' e ajusta o tamanho da mensagem
                int lastBrace = -1;
                for (uint8_t i = 0; i < recvLen; i++)
                {
                    if (buffer[i] == '}')
                        lastBrace = i;
                }

                if (lastBrace >= 0)
                {
                    // Garante null-termination logo após o último '}'
                    if (lastBrace + 1 < len)
                        buffer[lastBrace + 1] = '\0';
                    else if (len > 0)
                        buffer[len - 1] = '\0';
                    recvLen = lastBrace + 1;
                }
                else
                {
                    // Não encontrou '}', faz null-termination padrão
                    if (recvLen > 0 && recvLen < len)
                        buffer[recvLen] = '\0';
                    else if (len > 0)
                        buffer[len - 1] = '\0';
                }

                // Filtra caracteres inválidos no buffer sem alterar o conteúdo válido
                for (uint8_t i = 0; i < recvLen; i++)
                {
                    if (buffer[i] < 32 || buffer[i] > 126)
                    {
                        buffer[i] = ' '; // Substitui caracteres inválidos por espaço
                    }
                }

                // Log detalhado do buffer após limpeza

                // Garante null-termination no final do buffer
                if (recvLen < len)
                {
                    buffer[recvLen] = '\0';
                }
                else if (len > 0)
                {
                    buffer[len - 1] = '\0';
                }

#ifdef DEBUG_ON
                char msg[64];
                snprintf(msg, sizeof(msg), "From: %d To: %d id: %d", rf95.headerFrom(), rf95.headerTo(), rf95.headerId());
                Logger::info(msg);
#endif
                len = recvLen;
                return true;
            }
        }
        return false;
    }

    int packetRssi() override
    {
        return rf95.lastRssi();
    }

    void setHeaderTo(uint8_t tid) override
    {
        _tid = tid;
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
        return sendMessage(_tid, message);
    }
    bool available() override
    {
        return rf95.available();
    }
    byte read() override
    {
        uint8_t buffer[1] = {0};
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
    int headerFrom() override
    {
        return rf95.headerFrom();
    }
    int headerTo() override
    {
        return rf95.headerTo();
    }
    int headerId() override
    {
        return rf95.headerId();
    }
};

#endif
