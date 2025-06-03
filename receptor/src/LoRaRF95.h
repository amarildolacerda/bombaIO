#ifndef LORARF95_H
#define LORARF95_H

#ifdef RF95

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"
#include "queue_message.h"
#include "LoRaInterface.h"

#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

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
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        rf95.setTxPower(14, false);
        return connected;
    }

        uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }

private:
    void setState(LoRaStates st) override
    {
        state = st;
        switch (state)
        {
        case LoRaRX:
            Serial.println("LoRaRX");
            rf95.setModeRx();
            rxDelay = millis();
            break;
        case LoRaTX:
            Serial.println("LoRaTX");
            rf95.setModeTx();
            txDelay = millis();
            break;
        case LoRaWAITING:
            Serial.println("LoRaWAITING");
            setState(LoRaRX);
            break;
        case LoRaIDLE:
            setState(LoRaRX);
            break;
        default:
            break;
        }
    }

    bool parseRecv(char *buf, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = rf95.headerTo();
        rec.from = rf95.headerFrom();
        rec.id = rf95.headerId();
        rec.hope = rf95.headerFlags();

        if (buf == NULL || buf[0] != '{' || buf[len - 1] != '}')
        {
            // Serial.println("Mensagem mal formatada");
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
            sprintf(rec.value, '\0');
        }
        return true;
    }

    bool receiveMessage() override
    {
        if (!rf95.available())
        {
            return false;
        }

        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t recvLen = sizeof(buffer);

        memset(buffer, 0, recvLen);

        if (rf95.recv((uint8_t *)buffer, &recvLen))
        {
            buffer[recvLen] = '\0';

            if (!isValidMessage(buffer, recvLen))
            {
                Logger::error("Invalid message: %s ", buffer + 1);
                ackIf(false);
                return false;
            }

            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            headerSender = (uint8_t)buffer[0];

            if (headerSender == terminalId || mfrom == terminalId)
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

            if (strstr(buffer, "ping") != NULL)
            {
                if (mto != terminalId)
                    return false;
                txQueue.push(mfrom, "pong", terminalName, terminalId, ALIVE_PACKET, nHeaderId++);
                return true;
            }
            else if (strstr(buffer, "pong") != NULL)
            {
                if (!isDeviceActive(mfrom))
                {
                    Logger::log(LogLevel::INFO, "Pong received from: %d", mfrom);
                    setDeviceActive(mfrom);
                }
                return false;
            }
            else if (strstr(buffer, "time") != NULL)
            {
                return false;
            }

            if ((mto == terminalId) || (mto == 0xFF))
            {
                //                Logger::log(LogLevel::DEBUG, "Direct message [%d->%d]: %s", mfrom, mto, buffer);
                Logger::log(LogLevel::RECEIVE, "(%d)[%X->%X:%X](%d) %s", terminalId, mfrom, mto, rf95.headerId(), rf95.headerFlags(), buffer);
                if (mto == terminalId)
                    return true;
            }

            // #ifdef MESH
            uint8_t salto = rf95.headerFlags();
            if (salto > 1 && salto <= ALIVE_PACKET)
            {
                if (mto != terminalId && !isDeviceActive(mfrom))
                {
                    salto--;

                    MessageRec rec;
                    if (parseRecv(buffer, recvLen, rec))
                    {
                        rec.hope = salto;
                        txQueue.pushItem(rec);
                        Serial.print("MESH From: ");
                        Serial.println(rf95.headerFrom());
                    }
                }
                return mto == 0xFF;
            }
            // #endif
            return _promiscuos || (mto == 0xFF);
        }
        return false;
    }

    bool sendMessage(MessageRec &rec) override
    // uint8_t terminalTo, const char *message, uint8_t terminalFrom,
    //                 uint8_t hope, uint8_t seq)
    {
        char message[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "%s|%s", rec.event, rec.value);
        Serial.println(message);
        if (len == 0 || len > Config::MESSAGE_MAX_LEN)
        {
            Logger::error("Invalid message size");
            return false;
        }

        rf95.setModeTx();
        rf95.setHeaderFrom(rec.from);
        rf95.setHeaderTo(rec.to);
        if (rec.id == 0)
            rec.id = ++nHeaderId;
        rf95.setHeaderId(rec.id);
        rf95.setHeaderFlags(rec.hope, 0xFF);

        char fullMessage[Config::MESSAGE_MAX_LEN];
        memset(fullMessage, 0, sizeof(fullMessage));

        bool result = false;
        snprintf(fullMessage, sizeof(fullMessage), "%c{%s}", terminalId, message);
        len += 3;

        Logger::log(LogLevel::VERBOSE, "%s(%d)", fullMessage + 1, len);
        if (rf95.send((uint8_t *)fullMessage, len))
        {
            Serial.print("saitPacketSend ");
            if (rf95.waitPacketSent())
            {
                Logger::log(LogLevel::SEND, "(%d)[%X->%X:%X](%d) %s", terminalId, rec.from, rec.to, rec.id, rec.hope, message);
                result = true;
            }
        }
        Serial.println("End send ");

        return result;
    }

    void ackIf(bool ack = false)
    {
        if (rf95.headerTo() == terminalId)
        {
            txQueue.push(rf95.headerFrom(), ack ? "ack" : "nak", "", terminalId, ALIVE_PACKET);
        }
    }
};

static LoraRF lora;

#endif
#endif