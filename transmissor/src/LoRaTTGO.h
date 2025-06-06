#ifndef LORATTGO_H
#define LORATTGO_H

#ifdef TTGO

#include "LoRaInterface.h"
#include "config.h"
#include <LoRa.h>
#include "queue_message.h"

class LoRaTTGO : public LoRaInterface
{
private:
    long lastAvailable = 0;
    uint8_t headerTo = 0;
    uint8_t headerFrom = 0;
    uint8_t headerId = 0;
    uint8_t headerHope = 0;

public:
    void modeTx() override
    {
        // LoRa.idle();
    }
    void modeRx() override
    {
        LoRa.receive();
    }
    bool begin(const uint8_t terminal_Id, long frequency, bool promiscuous = true) override
    {
        terminalId = terminal_Id;
        LoRa.begin(frequency);

        switch (config)
        {
        case LORA_SLOW:
            // Config 1 (Long Range):
            LoRa.setSpreadingFactor(12);
            LoRa.setSignalBandwidth(125E3);
            LoRa.setCodingRate4(8);
            LoRa.setTxPower(23);

            break;
        case LORA_FAST:

            // Config 2 (Fast):
            LoRa.setSpreadingFactor(7);
            LoRa.setSignalBandwidth(250E3);
            LoRa.setCodingRate4(5);
            LoRa.setTxPower(14);
            break;
        default:

            // Config 3 (Balanced):
            LoRa.setSpreadingFactor(8);
            LoRa.setSignalBandwidth(125E3);
            LoRa.setCodingRate4(5);
            LoRa.setTxPower(14);
            break;
        }

        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setPreambleLength(8);
        _promiscuos = promiscuous;
        setState(LoRaRX);

        return true;
    }

    bool sendMessage(MessageRec &rec) // uint8_t tidTo, const char *message)
    {

        char message[Config::MESSAGE_MAX_LEN] = {0};
        snprintf(message, sizeof(message), "{%s|%s}", rec.event, rec.value);

        uint8_t len = strlen(message);

        LoRa.idle();
        LoRa.beginPacket();

        LoRa.write(rec.to);
        LoRa.write(rec.from);
        LoRa.write(rec.id);
        LoRa.write(rec.hope); // salto no mesh
        LoRa.write(terminalId);

        // char buffer[Config::MESSAGE_MAX_LEN] = {0};
        // buffer[0] = STX;
        // memcpy(buffer , message, len);
        // buffer[len + 1] = ETX;
        // buffer[len + 2] = '\0';

        int snd = LoRa.print(message);

        bool rt = LoRa.endPacket(true) > 0;

        Logger::log(LogLevel::SEND, "(%d)[%d→%d:%d] L: %d B: %s", terminalId, rec.from, rec.to, rec.id, len, message);

        return rt;
    }

    bool parseRecv(char *buf, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = headerTo;
        rec.from = headerFrom;
        rec.id = headerId;
        rec.hope = headerHope;

        if (buf == NULL || buf[0] != '{' || buf[len - 1] != '}')
        {
            return false;
        }

        char content[100];
        strncpy(content, buf + 1, len - 2);
        content[len - 2] = '\0';

        char *token = strtok(content, "|");
        if (token != nullptr)
        {
            int xe = snprintf(rec.event, sizeof(rec.event), token);
            rec.event[xe] = '\0';
        }

        token = strtok(nullptr, "|");
        if (token != nullptr)
        {
            int xv = snprintf(rec.value, sizeof(rec.value), token);
            rec.value[xv] = '\0';
        }
        else
        {
            sprintf(rec.value, "");
        }
        return true;
    }

    bool receiveMessage()
    {

        char buffer[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = 0;

        if (!LoRa.available())
            return false;

        int packetSize = LoRa.parsePacket();
        if (packetSize < 5)
            return false;

        headerTo = LoRa.read();
        headerFrom = LoRa.read();
        headerId = LoRa.read();
        headerHope = LoRa.read();
        headerSender = LoRa.read(); // terminal que enviou a mensagem
        memset(buffer, 0, sizeof(buffer));
        long rcvDelay = millis();
        bool pipe = false;
        while (len < packetSize - 5)
        {
            if (!LoRa.available() && millis() - rcvDelay > 100)
            {
                break;
            }
            rcvDelay = millis();
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
            if (r == '|')
                pipe = true;
            if (pipe && r == '}')
                break;
        }
        buffer[len] = '\0';

        Logger::warning("Received: %d,  [%d-%d] - %s", packetSize, headerFrom, headerTo, buffer);

        if ((headerFrom == terminalId || headerSender == terminalId))
            return false;

        MessageRec rec;

        bool parsed = parseRecv(buffer, len, rec);

        Logger::log(parsed ? LogLevel::RECEIVE : LogLevel::ERROR, "(%d)[%d→%d:%d](%d) L: %d B: %s",
                    headerSender, headerFrom, headerTo, headerId, headerHope,
                    len, buffer);

        if (!parsed)
            return false;

        bool result = (headerTo == 0xFF || headerTo == terminalId);
        if (result)
        {
            return rxQueue.pushItem(rec);
        }

        return result;
    }

    void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) override
    {
        Logger::info("Configuração do LoRa com CS: %d   RESET: %d  IRQ: %d", cs, reset, irq);
        LoRa.setPins(cs, reset, irq);
    }

    int packetRssi() override
    {
        return LoRa.packetRssi();
    }
    int packetSnr() override
    {
        return LoRa.packetSnr();
    }
};

#endif // LORATTGO_H
#endif