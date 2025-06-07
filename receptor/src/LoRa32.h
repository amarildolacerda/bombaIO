#ifndef LORA32_H
#define LORA32_H

#include "Arduino.h"
#include "queue_message.h"
#include "config.h"
#include "logger.h"
#include "LoRaInterface.h"
#include "stats.h"

#ifdef ESP32
#include "SPI.h"
#if defined(HELTEC)
#include "heltec.h"
#ifdef Heltec_LoRa
#define LoRa Heltec.LoRa
#else
#include "lora/LoRa.h"
#endif
#else
#include "LoRa.h"
#endif

class LoRa32 : public LoRaInterface
{
private:
    bool isHeltec;

    uint8_t _headerTo;
    uint8_t _headerFrom;
    uint8_t _headerId;
    uint8_t _headerSender;
    uint8_t _headerHope;
    long lastRcvMesssage;

public:
    LoRa32() : isHeltec(false) {}

    void modeTx() override
    {
    }
    void modeRx() override
    {
        LoRa.receive();
        checkConnectionHealth();
    }
    long currentFrequency = Config::LORA_BAND;
    void configParams()
    {
#ifdef HELTEC
        LoRa.setSpreadingFactor(9);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST);
        LoRa.setTxPowerMax(20);
        LoRa.setPreambleLength(8);
        LoRa.enableCrc();
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
#else
        LoRa.setSpreadingFactor(8);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setTxPower(14);
        LoRa.setPreambleLength(8);
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);

#endif
        setState(LoRaRX);
    }
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        currentFrequency = band;
        _promiscuos = promisc;
        terminalId = terminal_Id;

#ifdef HELTEC
        isHeltec = true;
        Heltec.begin(false, false, false);
        delay(100);
        // checkConnectionHealth();
        connected = LoRa.begin(band, false);
        configParams();

#else
        isHeltec = false;
        connected = LoRa.begin(band);
        configParams();
#endif
        Logger::log(LogLevel::INFO, "LoRa32 iniciado ");
        return true;
    }
    uint32_t lastCheck = 0;
    void checkConnectionHealth()
    {
        return;
        static int rssi = -157;
        if (millis() - lastCheck > 30000)
        { // A cada 1 minuto
            rssi = LoRa.packetRssi();
            if (rssi <= -157)
            {
                LoRa.sleep();
                delay(100);
                Logger::log(LogLevel::WARNING, "Tentando recuperar conexão LoRa...");
                connected = begin(terminalId, currentFrequency, _promiscuos);
                if (!connected)
                {
                    Serial.println("nao conectou LoRa");
                }
                configParams();
            }
            lastCheck = millis();
        }
    }

    bool sendMessage(MessageRec &rec) override
    {
        if (!connected)
            return false;
        stats.txCount++;
        char message[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "{%s|%s}", rec.event, rec.value);

        // LoRa.idle();
        LoRa.beginPacket();

        LoRa.write(rec.to);
        LoRa.write(rec.from);
        LoRa.write(rec.id);
        LoRa.write(rec.hope);
        LoRa.write(terminalId);

        LoRa.print(message);

        bool result = LoRa.endPacket(true) > 0;

        if (result)
        {
            stats.txSuccess++;
            Logger::log(LogLevel::SEND, "(%d)[%X→%X:%X](%d) %s",
                        terminalId, rec.from, rec.to, rec.id, rec.hope, message);

            // Atualiza métricas (assume sucesso no envio)
        }
        setState(LoRaIDLE);
        return result;
    }

    bool receiveMessage() override
    {
        stats.rxCount++;
        uint8_t packetSize = LoRa.parsePacket();
        if (packetSize == 0)
        {
            return false;
        }
        delay(10);

        _headerTo = LoRa.read();
        _headerFrom = LoRa.read();
        _headerId = LoRa.read();
        _headerHope = LoRa.read();
        _headerSender = LoRa.read();

        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t len = 0;
        bool passouPipe = false;
        long waitingRcv = millis();

        while (len <= packetSize + 5)
        {
            if (!LoRa.available() && (millis() - waitingRcv > 100))
            {
                break;
            }
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
            waitingRcv = millis();
        }
        stats.rxSuccess++;
        buffer[len] = '\0';

        MessageRec rec;
        if (!parseRecv(buffer, len, rec))
        {
            return false;
        }

        if (len == 0 || _headerFrom == terminalId || _headerSender == terminalId)
        {
            return false;
        }

        // Tratamento de mensagens especiais (ping/pong)
        if (strstr(buffer, EVT_PING) != NULL)
        {
            if (_headerTo != terminalId)
                return false;
            txQueue.push(_headerFrom, EVT_PONG, terminalName, terminalId, ALIVE_PACKET, _headerId);
            return true;
        }

        bool handled = (_headerTo == 0xFF || _headerTo == terminalId);

        if (handled)
        {
            if (!rxQueue.pushItem(rec))
            {
                handled = false;
            }
            Logger::log(handled ? LogLevel::RECEIVE : LogLevel::WARNING,
                        "(%d)[%X→%X:%X](%d) %s|%s",
                        _headerSender, _headerFrom, _headerTo, _headerId, _headerHope,
                        rec.event, rec.value);
        }

        return handled;
    }

    bool parseRecv(char *buf, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = _headerTo;
        rec.from = _headerFrom;
        rec.id = _headerId;
        rec.hope = _headerHope;

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
            snprintf(rec.event, sizeof(rec.event), token);
        }

        token = strtok(nullptr, "|");
        if (token != nullptr)
        {
            snprintf(rec.value, sizeof(rec.value), token);
        }
        else
        {
            rec.value[0] = '\0';
        }
        return true;
    }

    void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) override
    {
#ifndef HELTEC
        LoRa.setPins(cs, reset, irq);
        //  if (isHeltec)
        //      SPI.begin(5, 19, 27, 18); // SCK, MISO, MOSI, SS
#endif
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

static LoRa32 lora;
#endif
#endif