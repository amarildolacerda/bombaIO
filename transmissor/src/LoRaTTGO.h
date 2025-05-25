#ifndef LORATTGO_H
#define LORATTGO_H

#ifdef TTGO

#include "LoRaInterface.h"
#include "config.h"
#include <LoRa.h>

class LoRaTTGO : public LoRaInterface
{
private:
    uint8_t _tid = 0xFF;
    uint8_t _tidTo = 0xFF;
    long lastAvailable = 0;
    uint8_t hTo = 0;
    uint8_t hFrom = 0;
    uint8_t hId = 0;
    uint8_t hLive = 0;
    bool inPromiscuous = false;

public:
    bool beginSetup(float frequency, bool promiscuous = true) override
    {
        LoRa.begin(frequency);
        LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        LoRa.setSignalBandwidth(125E3); // 125kHz
        LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setTxPower(23);
        LoRa.setPreambleLength(8);

        inPromiscuous = promiscuous;
        LoRa.receive();

        return true;
    }

    bool sendMessage(uint8_t tidTo, const char *message) override
    {
        char m[251] = {0}; // Reduzido para evitar uso excessivo de memória

        LoRa.idle();
        LoRa.beginPacket();

        LoRa.write(tidTo > -1 ? tidTo : _tidTo);
        LoRa.write(_tid);
        LoRa.write(nHeaderId++);
        LoRa.write(3); // salto no mesh

        char msg[64];
        snprintf(msg, sizeof(msg), "Enviando de: %d para: %d bytes: %d", _tid, tidTo, strlen(message));
        Logger::log(LogLevel::SEND, msg);

        int snd = LoRa.print(message);

        bool rt = LoRa.endPacket() > 0;
        Logger::log(LogLevel::SEND, message);

        delay(50);
        LoRa.receive();
        return rt;
    }

    bool receiveMessage(uint8_t *buffer, uint8_t &len) override
    {
        if (!buffer)
            return false;
        len = 0;
        if (!LoRa.available())
            return false;

        hTo = LoRa.read();
        hFrom = LoRa.read();
        hId = LoRa.read();
        hLive = LoRa.read();
        buffer = {0};

        // memset(buffer, 0, sizeof(buffer));
        while (LoRa.available() && len < Config::MESSAGE_LEN - 1)
        {
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
        }
        buffer[len] = '\0';
        if ((hFrom == _tid) || (hTo == 0xFE))
            return false;

#ifdef DEBUG_ON
        char msg[64];
        snprintf(msg, sizeof(msg), "Term: (%d) From: %d To: %d id: %d len: %d Live: %d", _tid,
                 headerFrom(), headerTo(), headerId(), hLive,
                 strlen((char *)buffer));
        Logger::log(LogLevel::RECEIVE, msg);
        Logger::log(LogLevel::RECEIVE, (char *)buffer);
#endif
        if (!inPromiscuous)
        {

            return (hTo == 0xFF || hTo == _tid);
        }
        return true;
    }

    bool print(const char *message) override
    {
        return sendMessage(_tidTo, message);
    }
    void setHeaderTo(uint8_t tidTo) override
    {
        _tidTo = tidTo;
    }
    void setHeaderFrom(uint8_t tid) override
    {
        _tid = tid;
    }

    void setPins(int cs, int reset, int irq) override
    {
        Logger::info(String("Configuração do LoRa com CS: " + String(cs) + ", RESET: " + String(reset) + ", IRQ: " + String(irq)).c_str());
        LoRa.setPins(cs, reset, irq);
    }
    void endSetup() override
    {
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
        if (millis() - lastAvailable < 20)
        {
            return false;
        }
        int b = LoRa.parsePacket();
        lastAvailable = millis();
        return (b > 0);
    }
    int headerFrom() override { return hFrom; }
    int headerTo() override { return hTo; }
    int headerId() override { return hId; }
};

#endif // LORATTGO_H
#endif