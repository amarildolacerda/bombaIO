#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"
#include "queue.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
#endif

#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

//--------------------------------------------------RF
SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX

uint8_t noMesh[(MAX_MESH_DEVICES + 7) / 8] = {0}; // 32 bytes (256 bits)

// Funções de manipulação do bitmask
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

class LoraRF
{
private:
    RH_RF95<decltype(RFSerial)> rf95;
    uint8_t terminalId = 1;
    bool _promiscuos = false;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';
    uint8_t sender = 0xFF;
    uint8_t nHeaderId = 0;
    FifoList txQueue;
    FifoList rxQueue;

public:
    bool connected = false;

    LoraRF() : rf95(RFSerial) {}

    bool begin(uint8_t terminal_Id, long band, bool promisc = true)
    {
        terminalId = Config::TERMINAL_ID;
        RFSerial.begin(Config::LORA_SPEED);
        connected = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true);
        rf95.setFrequency(band);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        rf95.setTxPower(14, false);
        return connected;
    }

    bool processIncoming(MessageRec &rec)
    {
        return txQueue.pop(rec);
    }

    void send(uint8_t tid, String event, String value)
    {
        txQueue.push(tid, event, value, terminalId, ALIVE_PACKET, 0);
    }

    bool loop()
    {
        MessageRec rec;
        if (txQueue.pop(rec))
        {
            if (rec.event.indexOf("ack") >= 0 || rec.event.indexOf("nak") >= 0)
            {
                sendMessage(rec.to, rec.event.c_str(), rec.from);
                return true;
            }
            char msg[Config::MESSAGE_MAX_LEN];
            memset(msg, 0, sizeof(msg));
            snprintf(msg, sizeof(msg), "%s|%s", rec.event.c_str(), rec.value.c_str());
            sendMessage(rec.to, msg, rec.from, rec.hope, rec.id);
        }
        // receber para por na fila de entrada.
        if (rf95.available())
        {
            char buf[Config::MESSAGE_MAX_LEN];
            memset(buf, 0, sizeof(buf));
            uint8_t len = sizeof(buf);
            if (receiveMessage(buf, &len))
            {
                char *event; // Pega a primeira parte ("status")
                char *value; // Pega a segunda parte ("value")
                if (parseMessage(buf, len, event, value))
                    rxQueue.push(rf95.headerTo(), event, value, rf95.headerFrom());
            }
        }

        return true;
    }
    bool available()
    {
        return !rxQueue.isEmpty();
    }

    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }

private:
    bool parseMessage(char *buf, const uint8_t len, char *event, char *value)
    {
        char buffer[len + 1];
        strcpy(buffer, buf);
        // Divide a string usando '|' como delimitador
        event = strtok(buffer, "|"); // Pega a primeira parte ("status")
        value = strtok(NULL, "|");   // Pega a segunda parte ("value")
        if (value == NULL)
            sprintf(value, "ops");
        return event != NULL;
    }

    void ackIf(bool ack = false)
    {
        if (rf95.headerTo() == terminalId)
        {
            txQueue.push(rf95.headerFrom(), (ack) ? "ack" : "nak", "", terminalId);
        }
    }

    bool receiveMessage(char *buffer, uint8_t *len)
    {
        if (!rf95.waitAvailableTimeout(10))
        {
            return false;
        }

        uint8_t recvLen = Config::MESSAGE_MAX_LEN + 3;
        char localBuffer[recvLen] = {0};

        if (rf95.recv((uint8_t *)localBuffer, &recvLen))
        {
            localBuffer[recvLen] = '\0';

            if (!isValidMessage(localBuffer, recvLen))
            {
                Logger::error("Mensagem inválida. STX/ETX ausentes ou tamanho insuficiente");
                Serial.println(localBuffer);
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

            recvLen -= 3; // Remove STX, ETX e ID
            if (recvLen > Config::MESSAGE_MAX_LEN)
            {
                Logger::error("Mensagem muito longa");
                ackIf(false);
                return false;
            }

            strncpy(buffer, localBuffer + 2, recvLen);
            buffer[recvLen] = '\0';
            *len = recvLen;

            // Tratamento de ping/pong
            if (strstr(buffer, "ping") != NULL)
            {
                if (mto != terminalId)
                    return false;
                txQueue.push(mfrom, "pong", "0", terminalId);
                return true;
            }
            else

                if (strstr(buffer, "pong") != NULL)
            {
                if (!isDeviceActive(mfrom))
                {
                    Logger::log(LogLevel::INFO, "Recebido pong de: %d", mfrom);
                    setDeviceActive(mfrom);
                }
                return false;
            }
            else
            {
                if (strstr(buffer, "time") != NULL)
                    return false; // estava reiniciando
            }

            if ((mto == terminalId) || (mto == 0xFF))
            {
                Logger::log(LogLevel::DEBUG, "Mensagem direta [%d->%d]: %s", mfrom, mto, buffer);
                if (mto == terminalId)
                    return true;
            }

#ifdef MESH
            uint8_t salto = rf95.headerFlags();
            if (salto > 1 && salto <= ALIVE_PACKET)
            {
                if (mto != terminalId && !isDeviceActive(mfrom))
                {
                    salto--;

                    char *event; // Pega a primeira parte ("status")
                    char *value; // Pega a segunda parte ("value")
                    if (parseMessage(buffer, *len, event, value))
                        txQueue.push(rf95.headerTo(), event, value, rf95.headerFrom(), salto, rf95.headerId());
                }
            }
#endif
            return _promiscuos || (mto == 0xFF);
        }
        return false;
    }

    bool sendMessage(uint8_t terminalTo, const char *message, uint8_t terminalFrom,
                     uint8_t salto = ALIVE_PACKET, uint8_t seq = 0)
    {
        uint8_t len = strlen(message);
        if (len == 0 || len > Config::MESSAGE_MAX_LEN)
        {
            Logger::error("Tamanho de mensagem inválido para envio");
            return false;
        }

        rf95.setModeTx();
        rf95.setHeaderFrom(terminalFrom);
        rf95.setHeaderTo(terminalTo);
        rf95.setHeaderId(seq ? seq : ++nHeaderId);
        rf95.setHeaderFlags(salto, 0xFF);

        char fullMessage[Config::MESSAGE_MAX_LEN + 3];
        fullMessage[0] = terminalId;
        fullMessage[1] = STX;
        memcpy(fullMessage + 2, message, len);
        fullMessage[len + 2] = ETX;
        fullMessage[len + 3] = '\0';

        bool result = false;
        if (rf95.send((uint8_t *)fullMessage, len + 3))
        {
            if (rf95.waitPacketSent(MESSAGE_TIMEOUT_MS))
            {
                Logger::log(LogLevel::INFO, "Enviado %s (%d)[%d->%d](%d): %s", (terminalFrom != terminalId) ? "MESH" : "", terminalId, terminalFrom, terminalTo, salto, message);
                result = true;
            }
        }

        rf95.setModeRx();
        return result;
    }
};

static LoraRF lora;

#endif