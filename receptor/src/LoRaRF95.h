#ifndef LORARF95_H
#define LORARF95_H

#ifdef RF95

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"
#include "queue_message.h"
#include "LoRaInterface.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
#endif

#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN);

uint8_t noMesh[(MAX_MESH_DEVICES + 7) / 8] = {0};

void setDeviceActive(uint8_t deviceId)
{
    if (deviceId <= MAX_MESH_DEVICES)
    {
        noMesh[deviceId / 8] |= (1 << (deviceId % 8));
    }
}

bool isDeviceActive(uint8_t deviceId)
{
    if (deviceId > MAX_MESH_DEVICES)
        return false;
    return (noMesh[deviceId / 8] & (1 << (deviceId % 8))) != 0;
}

void clearDeviceActive(uint8_t deviceId)
{
    if (deviceId <= MAX_MESH_DEVICES)
    {
        noMesh[deviceId / 8] &= ~(1 << (deviceId % 8));
    }
}

bool isValidMessage(const char *msg, uint8_t len)
{
    return (len >= 4) && (msg[1] == '{') && (msg[len - 1] == '}');
}

class LoraRF : public LoRaInterface
{
private:
    RH_RF95<decltype(RFSerial)> rf95;
    bool _promiscuos = false;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';
    uint8_t sender = 0xFF;
    uint8_t nHeaderId = 0;
    FifoList txQueue;
    FifoList rxQueue;
    long txLoop = 0;
    long rxLoop = 0;

public:
    bool connected = false;

    LoraRF() : rf95(RFSerial) {}

    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;
        RFSerial.begin(Config::LORA_SPEED);
        connected = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true);
        rf95.setFrequency(band);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        rf95.setTxPower(14, false);
        return connected;
    }

    bool processIncoming(MessageRec &rec) override
    {
        bool result = false;

        result = rxQueue.pop(rec);
        return result;
    }

    // void send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId) override
    bool send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId) override
    {
        MessageRec msg;
        memset(&msg, 0, sizeof(MessageRec));
        msg.to = tid;
        msg.from = terminalId;
        msg.hope = ALIVE_PACKET;
        msg.id = ++nHeaderId;
        strncpy(msg.event, event, MAX_EVENT_LEN - 1);
        strncpy(msg.value, value, MAX_VALUE_LEN - 1);

        return txQueue.pushItem(msg);
    }

    bool loop() override
    {
        bool processed = false;

        // Processar envios
        if (millis() - txLoop > 5)
        {
            txLoop = millis();
            MessageRec rec;

            bool hasItem = txQueue.pop(rec);

            if (hasItem)
            {
                char msg[Config::MESSAGE_MAX_LEN];
                memset(msg, 0, sizeof(msg));
                snprintf(msg, sizeof(msg), "%s|%s", rec.event, rec.value);
                sendMessage(rec.to, msg, rec.from, rec.hope, rec.id);
            }
        }

        // Processar recebimentos
        if (millis() - rxLoop > 5)
        {
            rxLoop = millis();
            if (rf95.available())
            {
                char buf[Config::MESSAGE_MAX_LEN];
                memset(buf, 0, sizeof(buf));
                uint8_t len = sizeof(buf);
                if (receiveMessage(buf, &len))
                {
                    MessageRec rec;
                    if (parseRecv(buf, len, rec))
                    {
                        return rxQueue.pushItem(rec);
                    }
                }
                processed = true;
            }
        }

        return processed;
    }

    bool available() override
    {
        return !rxQueue.isEmpty();
    }

    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }

private:
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

    bool receiveMessage(char *buffer, uint8_t *len)
    {
        if (!rf95.available())
        {
            return false;
        }

        uint8_t recvLen = Config::MESSAGE_MAX_LEN + 3;
        char localBuffer[recvLen];
        memset(localBuffer, 0, recvLen);

        if (rf95.recv((uint8_t *)localBuffer, &recvLen))
        {
            localBuffer[recvLen] = '\0';

            if (!isValidMessage(localBuffer, recvLen))
            {
                Logger::error("Invalid message");
                ackIf(false);
                return false;
            }

            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            sender = (uint8_t)localBuffer[0];

            if (sender == terminalId || mfrom == terminalId)
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

            strncpy(buffer, localBuffer + 2, recvLen);
            buffer[recvLen] = '\0';
            *len = recvLen;

            if (strstr(buffer, "ping") != NULL)
            {
                if (mto != terminalId)
                    return false;
                txQueue.push(mfrom, "pong", "0", terminalId, ALIVE_PACKET, nHeaderId++);
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
                    if (parseRecv(buffer, *len, rec))
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

    bool sendMessage(uint8_t terminalTo, const char *message, uint8_t terminalFrom,
                     uint8_t hope, uint8_t seq)
    {
        uint8_t len = strlen(message);
        if (len == 0 || len > Config::MESSAGE_MAX_LEN)
        {
            Logger::error("Invalid message size");
            return false;
        }

        rf95.setModeTx();
        rf95.setHeaderFrom(terminalFrom);
        rf95.setHeaderTo(terminalTo);
        rf95.setHeaderId(seq);
        rf95.setHeaderFlags(hope, 0xFF);

        char fullMessage[Config::MESSAGE_MAX_LEN + 3];
        memset(fullMessage, 0, sizeof(fullMessage));
        fullMessage[0] = terminalId;
        fullMessage[1] = STX;
        strncpy(fullMessage + 2, message, len);
        fullMessage[len + 2] = ETX;

        bool result = false;
        uint32_t start = millis();
        while (!result && (millis() - start < MESSAGE_TIMEOUT_MS))
        {
            if (rf95.send((uint8_t *)fullMessage, len + 3))
            {
                if (rf95.waitPacketSent(MESSAGE_TIMEOUT_MS))
                {
                    Logger::log(LogLevel::SEND, "(%d)[%X->%X:%X](%d) %s", terminalId, terminalFrom, terminalTo, seq, hope, message);
                    result = true;
                }
            }
            delay(10);
        }

        rf95.setModeRx();
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