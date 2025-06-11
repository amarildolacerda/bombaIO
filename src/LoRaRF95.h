#ifndef LORARF95_H
#define LORARF95_H

#ifdef RF95

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"
#include "queue_message.h"
#include "RadioInterface.h"
#include "app_messages.h"
#include "stats.h"

#ifdef ESP8266

static RH_RF95<HardwareSerial> rf95(Serial);

#else

#include <SoftwareSerial.h>
SoftwareSerial SSerial(RX_PIN, TX_PIN); // RX, TX
#define COMSerial SSerial
#define ShowSerial Serial

static RH_RF95<SoftwareSerial> rf95(COMSerial);
#endif

class LoRaRF95 : public RadioInterface
{
private:
public:
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
#ifdef ESP8266
        Serial.swap();
#endif
        terminalId = terminal_Id;
        connected = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true);
        rf95.setFrequency(band);

        // rf95.setModemConfig(rf95.Bw125Cr45Sf128);
        rf95.setTxPower(14, false);
        rf95.setPreambleLength(8);

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
    uint8_t _headerHope = 0;
    void modeRx() override
    {
        rf95.setModeRx();
    }
    void modeTx() override
    {
        rf95.setModeTx();
    }

    bool receiveMessage() override
    {

        stats.rxCount++;
        if (!rf95.waitAvailableTimeout(50))
        {
            return false;
        }

        char buffer[MESSAGE_MAX_LEN];
        uint8_t recvLen = sizeof(buffer);

        memset(buffer, 0, recvLen);

        if (rf95.recv((uint8_t *)buffer, &recvLen))
        {
            stats.rxSuccess++;
            buffer[recvLen] = '\0';

            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            _headerHope = (uint8_t)buffer[0];

            if (/*headerSender == terminalId ||*/ mfrom == terminalId)
            {
                return false;
            }

            recvLen -= 3;
            if (recvLen > MESSAGE_MAX_LEN)
            {
                Logger::error("Message too long");
                ackIf(false);
                return false;
            }

            MessageRec rec;
            bool parsed = parseRcv(rec, buffer + 1);
            if (!parsed)
            {
                // printHex(buffer + 1);
                return false; // bloqueia caracteres invalidos
            }
            rec.hope = _headerHope;

            if (strstr(buffer, EVT_PING) != NULL)
            {
                if (mto != terminalId)
                    return false;
                txQueue.push(mfrom, EVT_PONG, terminalName, terminalId, ALIVE_PACKET, nHeaderId++);
                return true;
            }

            if ((mto == terminalId) || (mto == 0xFF))
            {
                Logger::log(LogLevel::RECEIVE, "[%X->%X:%X](%d) %s|%s", mfrom, mto, rf95.headerId(), rf95.headerFlags(), rec.event, rec.value);
                rxQueue.push(rec.to, rec.event, rec.value, rec.from, rec.hope, rec.id);
                if (mto == terminalId)
                    return true;
            }

            meshMessage(rec);

            return _promiscuos || (mto == 0xFF);
        }
        return false;
    }

    bool sendMessage(MessageRec &rec) override
    {
        stats.txCount++;
        char message[MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "%c{%s|%s}", rec.hope, rec.event, rec.value);

        if (len == 0 || len > MESSAGE_MAX_LEN)
        {
            return false;
        }

        // 2. Configurar rádio
        rf95.setModeTx();

        rf95.setHeaderFrom(rec.from);
        rf95.setHeaderTo(rec.to);
        rf95.setHeaderId(rec.id);
        rf95.setHeaderFlags(len, 0xFF);

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
                    Logger::log(LogLevel::SEND, String((rec.hope < 3 ? "MESH " : "") + String("[%X->%X:%X](%d) %s")).c_str(), rec.from,
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

#endif
#endif