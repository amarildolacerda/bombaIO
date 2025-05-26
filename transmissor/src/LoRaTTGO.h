#ifndef LORATTGO_H
#define LORATTGO_H

#ifdef TTGO

#include "LoRaInterface.h"
#include "config.h"
#include <LoRa.h>

class LoRaTTGO : public LoRaInterface
{
private:
    uint8_t terminalId = 0xFF;
    uint8_t _tidTo = 0xFF;
    long lastAvailable = 0;
    uint8_t hTo = 0;
    uint8_t hFrom = 0;
    uint8_t hId = 0;
    uint8_t hLive = 0;
    uint8_t sender = 0xFF;
    bool inPromiscuous = false;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';

public:
    bool beginSetup(float frequency, bool promiscuous = true) override
    {
        LoRa.begin(frequency);
        LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        LoRa.setSignalBandwidth(125E3); // 125kHz
        LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setTxPower(14);
        LoRa.setPreambleLength(8);

        inPromiscuous = promiscuous;
        LoRa.receive();

        return true;
    }

    bool sendMessage(uint8_t tidTo, const char *message) override
    {

#ifdef DEBUG_ON
        Serial.print("entrou LoRa.sendmessage()");
#endif
        LoRa.idle();
        LoRa.beginPacket();

        LoRa.write(tidTo > -1 ? tidTo : _tidTo);
        LoRa.write(terminalId);
        LoRa.write(nHeaderId++);
        uint8_t len = strlen(message);
        LoRa.write(3); // salto no mesh
        LoRa.write(terminalId);

        char buffer[Config::MESSAGE_LEN] = {0};
        buffer[0] = STX;
        memcpy(buffer + 1, message, len);
        buffer[len] = ETX;
        buffer[len + 1] = '\0';

        int snd = LoRa.print(buffer);

        bool rt = LoRa.endPacket() > 0;

        char msg[Config::MESSAGE_LEN + 64];
        snprintf(msg, sizeof(msg), "[%d→%d] L: %d B: %s", terminalId, tidTo, strlen(buffer), buffer);
        Logger::log(LogLevel::SEND, msg);

        delay(50);
        LoRa.receive();
#ifdef DEBUG_ON
        Serial.println("saindo LoRa.sendmessage()");
#endif
        return rt;
    }

    bool receiveMessage(uint8_t *buffer, uint8_t &len) override
    {

        if (!buffer)
            return false;
#ifdef DEBUG_ON
        Serial.print("entrou receiveMessage");
#endif
        len = 0;
        if (!LoRa.available())
            return false;

        hTo = LoRa.read();
        hFrom = LoRa.read();
        hId = LoRa.read();
        hLive = LoRa.read();
        sender = LoRa.read(); // terminal que enviou a mensagem
        memset(buffer, 0, sizeof(buffer));
        while (LoRa.available() && len < Config::MESSAGE_LEN - 1)
        {
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
        }
        buffer[len] = '\0';
        if (len == 0 || sender == terminalId)
            return false;

        Serial.print("RxSender: ");
        Serial.print(sender);
        Serial.print(" ");
        Serial.println((char *)buffer);

        Serial.print("HEX: ");
        for (size_t i = 0; i < len; i++)
        {
            if (buffer[i] < 0x10)
                Serial.print('0'); // zero padding para valores < 0x10
            Serial.print(buffer[i], HEX);
            Serial.print(' ');
        }
        Serial.println();

        if (buffer[len - 1] != ETX && buffer[len - 1] != '\n')
        {
            // Se o último caractere não for '}' ou '\n', é um erro de formatação
            Serial.println("Erro: mensagem incompleta ou mal formatada.");
            Serial.println((char *)buffer);
            return false;
        }
        buffer[--len] = '\0'; // retira o }
        if (len > 1 && buffer[0] == STX)
        {
            for (int i = 1; i < len; i++)
            {
                buffer[i - 1] = buffer[i];
            }
        }
        buffer[--len] = '\0'; // retira o }
        Serial.println((char *)buffer);

        if ((hFrom == terminalId || sender == terminalId))
        {
#ifdef DEBUG_ON
            Serial.println("saindo false receivemessage");
#endif
            return false;
        }

        char msg[Config::MESSAGE_LEN + 64];
        snprintf(msg, sizeof(msg), "[%d→%d] L: %d Live: %d B: %s",
                 headerFrom(), headerTo(), hLive,
                 strlen((char *)buffer), buffer);
        Logger::log(LogLevel::RECEIVE, msg);

        if (!inPromiscuous)
        {

#ifdef DEBUG_ON
            Serial.println("saiu receivemessage");
#endif

            return (hTo == 0xFF || hTo == terminalId);
        }
#ifdef DEBUG_ON
        Serial.println("saindo  true receivemessage");
#endif
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
        terminalId = tid;
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