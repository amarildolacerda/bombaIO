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

public:
    bool beginSetup(float frequency, bool promiscuous = true) override
    {
        LoRa.begin(frequency);
        // LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        // LoRa.setSignalBandwidth(125E3); // 125kHz
        // LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        // LoRa.setTxPower(14);
        // LoRa.setPreambleLength(8);
        return true;
    }

    bool sendMessage(uint8_t tidTo, const char *message) override
    {
        char m[251] = {0}; // Reduzido para evitar uso excessivo de memória
        m[0] = tidTo > -1 ? tidTo : _tidTo;
        m[1] = _tid;
        m[2] = genHeaderId();
        m[3] = 0xFF;                            // 0x00 = no future
        strncpy(m + 4, message, sizeof(m) - 5); // Usar strncpy para evitar buffer overflow

        LoRa.beginPacket();
        LoRa.write((uint8_t *)m, strlen(m));
        bool rt = LoRa.endPacket() > 0;

        delay(10);
        return rt;
    }

    bool receiveMessage(uint8_t *buffer, uint8_t &len) override
    {
        // int packetSize = LoRa.parsePacket();
        // Serial.print("Receive ");
        // Serial.println(packetSize);
        // if (packetSize > 0)
        // {
        len = 0;
        while (LoRa.available() && len < Config::MESSAGE_LEN - 1)
        {
            buffer[len++] = (char)LoRa.read();
        }
        buffer[len] = '\0';
        if (len > 4)
        {
            hTo = buffer[0];
            hFrom = buffer[1];
            hId = buffer[2];
        }
        // Remove os 4 primeiros bytes de controle do buffer
        for (uint8_t i = 0; i < len - 4; ++i)
        {
            buffer[i] = buffer[i + 4];
        }
        len = len > 4 ? len - 4 : 0;
        buffer[len] = '\0';

        Serial.println((char *)buffer);
#ifdef DEBUG_ON
        char msg[64];
        snprintf(msg, sizeof(msg), "From: %d To: %d id: %d", headerFrom(), headerTo(), headerId());
        Logger::info(msg);
#endif

        return true;
        //}
        // return false;
    }

    bool print(const char *message) override
    {
        return sendMessage(0xFF, message);
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
        // int a = LoRa.available();
        if (b > 0)
        {
            //  Serial.print("check available: ");
            //  Serial.print(a);
            Serial.print(" packet ");
            Serial.print(b);
            Serial.println("");
        }
        lastAvailable = millis();
        return (b > 0);
    }
    int headerFrom() override { return hFrom; }
    int headerTo() override { return hTo; }
    int headerId() override { return hId; }
};

#endif // LORATTGO_H
#endif