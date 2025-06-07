#ifndef LORARF95_H
#define LORARF95_H

#ifdef RF95

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"
#include "queue_message.h"
#include "LoRaInterface.h"
#include "app_messages.h"
#include "stats.h"

#include <SoftwareSerial.h>
SoftwareSerial SSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial

RH_RF95<SoftwareSerial> rf95(COMSerial);

class LoraRF : public LoRaInterface
{
private:
public:
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;
        connected = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true);
        rf95.setFrequency(band);

        switch (config)
        {
        case LORA_SLOW:
            rf95.setModemConfig(rf95.Bw125Cr48Sf4096);
            rf95.setTxPower(14, false);
            rf95.setPreambleLength(9);

            break;
        case LORA_FAST:
            rf95.setModemConfig(rf95.Bw500Cr45Sf128);
            rf95.setTxPower(14, false);
            rf95.setPreambleLength(8);
            break;
        default:
            // rf95.setModemConfig(rf95.Bw125Cr45Sf128);
            rf95.setTxPower(14, false);
            rf95.setPreambleLength(8);

            break;
        }

        return connected;
    }

    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    int packetRssi() override
    {
        return rf95.lastRssi();
    }

private:
    void modeRx() override
    {
        rf95.setModeRx();
    }
    void modeTx() override
    {
        rf95.setModeTx();
    }
    bool parseRecv(char *buf, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        len = strlen(buf);
        // Serial.print(len);
        // Serial.print(":");
        // Serial.println(buf);

        rec.to = rf95.headerTo();
        rec.from = rf95.headerFrom();
        rec.id = rf95.headerId();
        rec.hope = rf95.headerFlags();

        if (buf == NULL || buf[0] != '{' || buf[len - 1] != '}')
        {
            Logger::error("Mensagem mal formatada ");
            // Serial.println(buf);
            return false;
        }

        // Copia o conteúdo interno (sem as chaves)
        char content[100];
        strncpy(content, buf + 1, len - 2);
        content[len - 2] = '\0';

        // Separar event e value usando '|'
        char *token = strtok(content, "|");
        if (token != nullptr)
        {
            int xe = snprintf(rec.event, sizeof(rec.event), token);
            rec.event[xe] = '\0'; // Adicionar o terminador nulo
        }

        token = strtok(nullptr, "|");
        if (token != nullptr)
        {
            int xv = snprintf(rec.value, sizeof(rec.value), token);
            rec.value[xv] = '\0'; // Adicionar o terminador nulo
        }
        else
        {
            rec.value[0] = '\0';
            // sprintf(rec.value, '\0');
        }

        return true;
    }

    void printHex(const char *msg)
    {
        if (msg == NULL)
        {
            Serial.println("(NULL)");
            return;
        }

        for (uint8_t i = 0; msg[i] != '\0'; i++)
        {
            uint8_t val = msg[i]; // Garante tratamento como byte unsigned
            if (val < 0x10)
                Serial.print("0");
            Serial.print(val, HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    bool receiveMessage() override
    {

        stats.rxCount++;
        if (!rf95.waitAvailableTimeout(50))
        {
            return false;
        }

        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t recvLen = sizeof(buffer);

        memset(buffer, 0, recvLen);

        if (rf95.recv((uint8_t *)buffer, &recvLen))
        {
            stats.rxSuccess++;
            buffer[recvLen] = '\0';

            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            headerSender = (uint8_t)buffer[0];

            if (/*headerSender == terminalId ||*/ mfrom == terminalId)
            {
                return false;
            }

            recvLen -= 3;
            if (recvLen > Config::MESSAGE_MAX_LEN)
            {
                Logger::error("Message too long");
                ackIf(false);
                return false;
            }

            MessageRec rec;
            bool parsed = parseRecv(buffer + 1, recvLen - 1, rec);
            if (!parsed)
            {
                printHex(buffer + 1);
                return false; // bloqueia caracteres invalidos
            }

            if (strstr(buffer, EVT_PING) != NULL)
            {
                if (mto != terminalId)
                    return false;
                txQueue.push(mfrom, EVT_PONG, terminalName, terminalId, ALIVE_PACKET, nHeaderId++);
                return true;
            }

            if ((mto == terminalId) || (mto == 0xFF))
            {
                Logger::log(LogLevel::RECEIVE, "(%d)[%X->%X:%X](%d) %s|%s", terminalId, mfrom, mto, rf95.headerId(), rf95.headerFlags(), rec.event, rec.value);
                rxQueue.push(rec.to, rec.event, rec.value, rec.from, rec.hope, rec.id);
                if (mto == terminalId)
                    return true;
            }

            // #ifdef MESH
            uint8_t salto = rf95.headerFlags();
            if (salto > 0 && salto <= ALIVE_PACKET)
            {
                if (mto != terminalId)
                {
                    salto--;
                    rec.hope = salto;
                    txQueue.push(rec.to, rec.event, rec.value, rec.from, rec.hope, rec.id);
                }
                return mto == 0xFF;
            }
            // #endif
            return _promiscuos || (mto == 0xFF);
        }
        return false;
    }

    bool sendMessage(MessageRec &rec) override
    {
        stats.txCount++;
        char message[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "%c{%s|%s}", terminalId, rec.event, rec.value);

        if (len == 0 || len > Config::MESSAGE_MAX_LEN)
        {
            return false;
        }

        // 2. Configurar rádio
        rf95.setModeTx();

        rf95.setHeaderFrom(rec.from);
        rf95.setHeaderTo(rec.to);
        rf95.setHeaderId(rec.id);
        rf95.setHeaderFlags(rec.hope, 0xFF);

        // 4. Tentar enviar
        bool sendResult = false;
        for (int attempt = 0; attempt < 3 && !sendResult; attempt++)
        {
            if (rf95.send((uint8_t *)message, len))
            {
                if (rf95.waitPacketSent(1000))
                { // Timeout de 5 segundos
                    stats.txSuccess++;
                    sendResult = true;
                    Logger::log(LogLevel::SEND, "(%d)[%X->%X:%X](%d) %s", terminalId, rec.from,
                                rec.to, rec.id, rec.hope, message + 1);

                    break;
                }
                else
                {
                    Logger::error("Timeout no waitPacketSent");
                }
            }
            else
            {
                Logger::error("Falha no send()");
            }
        }
        return sendResult;
    }
    void ackIf(bool ack = false)
    {
        if (rf95.headerTo() == terminalId)
        {
            txQueue.push(rf95.headerFrom(), ack ? EVT_ACK : EVT_NAK, "", terminalId, ALIVE_PACKET);
        }
    }
};

static LoraRF lora;

#endif
#endif