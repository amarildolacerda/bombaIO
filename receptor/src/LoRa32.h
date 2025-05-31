#ifndef LORA32_H
#define LORA32_H

#include "Arduino.h"
#include "queue_message.h"
#include "config.h"

#include "logger.h"
#include "LoRaInterface.h"

#ifdef ESP32
#if defined(HELTEC)
//-----------------------------------------------
// #include "LoRaWan_APP.h"
#include "Arduino.h"
#include "heltec.h"
#include "lora/LoRa.h"

#define TX_OUTPUT_POWER 14 // dBm

#define LORA_BANDWIDTH 0        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 7 // [SF7..SF12]
#define LORA_CODINGRATE 1       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#else
//---------------------------------------------------------------
#include "LoRa.h"
#endif
#endif

//==========================================================

enum LoRaStates
{
    LoRaTX,
    LoRaRX,
    LoRaWAITING
};

class LoRa32 : public LoRaInterface
{
private:
    FifoList txQueue;
    FifoList rxQueue;
    bool inPromiscuous = true;

    const char bEOF = '}';
    const char bBOF = '{';
    const char bSEP = '|';
    LoRaStates state = LoRaRX;
    uint8_t terminalId = 0;

    void setHeaderFrom(const uint8_t tid) override
    {
        terminalId = tid;
    }
    void setState(const LoRaStates st)
    {
        state = st;
        switch (st)
        {
        case LoRaRX:
            LoRa.receive();
            rxDelay = millis();
            // Serial.println("To RX");
            break;
        case LoRaTX:
            // Serial.println("To TX");
            LoRa.idle();
            break;
        case LoRaWAITING:
            // Serial.println("To WAITING");
            break;
        default:
            state = LoRaRX;
            LoRa.receive();
            break;
        }
    }

public:
    bool connected = false;
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        inPromiscuous = promisc;

#ifdef HELTEC
        // LoRa.setPins();
        LoRa.begin(868E6 /*band*/, false);
        LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        LoRa.setSignalBandwidth(125E3); // 125kHz
        LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setTxPowerMax(14);
        LoRa.setPreambleLength(8);

#else
        LoRa.begin(band);

        LoRa.setSpreadingFactor(7);     // Padrão é 7 (6-12)
        LoRa.setSignalBandwidth(125E3); // 125kHz
        LoRa.setCodingRate4(5);         // 4/5 coding rate
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);

        LoRa.setTxPower(14);
        LoRa.setPreambleLength(8);

#endif
        setState(LoRaRX);
        Serial.println("LoRa Iniciado");
        return true;
    }
    long rxDelay = 0;
    long txDelay = 0;
    bool loop() override
    {
        bool handled = false;
        switch (state)
        {
        case LoRaRX:
            if (LoRa.available())
                if (millis() - rxDelay > 5)
                {
                    rxDelay = millis();
                    handled = receiveMessage();
                }
            if (txQueue.size() > 0)
                setState(LoRaTX);

            break;
        case LoRaTX:
            if (millis() - txDelay > 5)
            {
                txDelay = millis();

                MessageRec txRec;

                if (txQueue.pop(txRec))
                {
                    handled = sendMessage(txRec.to, txRec.event, txRec.value, txRec.from, txRec.hope, txRec.id);
                }
            }

            break;
        case LoRaWAITING:

#ifdef HELTEC

            if (LoRa.endPacket(true) > 0)
            {
                Serial.print("RX ....");
                setState(LoRaRX);
                handled = true;
                return true;
            }
#else
            if (LoRa.endPacket(false) > 0)
            {
                setState(LoRaRX);
                handled = true;
            }
#endif
            if (millis() - txDelay > 100)
            {
                txDelay = millis();
                setState(LoRaRX);
            }
            break;
        default:
            Serial.println("default");
            setState(LoRaRX);
            break;
        }
        return handled;
    }

    uint8_t seq = 0;
    bool send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId) override
    {
        MessageRec rec;
        rec.to = tid;
        rec.from = terminalId;
        snprintf(rec.event, sizeof(rec.event) - 1, event);
        snprintf(rec.value, sizeof(rec.value) - 1, value);
        rec.hope = 3;
        rec.id = ++seq;
        return txQueue.pushItem(rec);
    }
    void printHex(const char *msg, const uint8_t len) const
    {
        for (int i = 0; i < len; i++)
        {
            if (msg[i] < 0x10)
                Serial.print("0");
            Serial.print(msg[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    bool processIncoming(MessageRec &rec) override
    {
        return rxQueue.pop(rec);
    }
    void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) override
    {
        LoRa.setPins(cs, reset, irq);
    }
    bool available() override
    {
        return !rxQueue.isEmpty();
    }

    uint8_t _headerTo;
    uint8_t _headerFrom;
    uint8_t _headerId;
    uint8_t _headerSender;
    uint8_t _headerHope;

    bool sendMessage(uint8_t tidTo, const char *event, const char *value, const uint8_t tidFrom, uint8_t hope, uint8_t id)
    {
        char message[Config::MESSAGE_MAX_LEN] = {0};
        int x = sprintf(message, "%s|%s", event, value);
        message[x] = '\0';

        uint8_t len = strlen(message);

        LoRa.idle();
        LoRa.beginPacket();
        uint8_t sq = seq++;
        LoRa.write(tidTo);
        LoRa.write(tidFrom);
        LoRa.write(sq);
        LoRa.write(3); // salto no mesh
        LoRa.write(terminalId);

        char buffer[Config::MESSAGE_MAX_LEN] = {0};
        buffer[0] = '{';
        memcpy(buffer + 1, message, len);
        buffer[len + 1] = '}';
        buffer[len + 2] = '\0';

        int snd = LoRa.print(buffer);

        // bool rt = LoRa.endPacket() > 0;

        Logger::log(LogLevel::SEND, "(%d)[%X→%X:%X](%d) L: %d B: %s", terminalId, tidFrom, tidTo, sq, hope, snd, buffer);

        setState(LoRaWAITING);
        return snd > 0;
    }

    long lastRcvMesssage;
    bool receiveMessage()
    {

        if (!LoRa.available())
            return false;

        uint8_t packetSize = LoRa.parsePacket();
        if (packetSize < 5)
        {
            return false;
        }

        char buffer[Config::MESSAGE_MAX_LEN];

        uint8_t len = 0;
        _headerTo = LoRa.read();
        _headerFrom = LoRa.read();
        _headerId = LoRa.read();
        _headerHope = LoRa.read();
        _headerSender = LoRa.read(); // terminal que enviou a mensagem
        memset(buffer, 0, sizeof(buffer));
        bool passouPipe = false;
        while (LoRa.available() && len <= packetSize + 5)
        {
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
            if (r == '|')
                passouPipe = true;
            if (passouPipe && r == '}')
                break;
        }
        buffer[len] = '\0';

        if (len == 0 || _headerFrom == terminalId || _headerSender == terminalId)
            return false;
        Logger::log(LogLevel::RECEIVE, "Recebendo: %d bytes", packetSize);

        MessageRec rec;
        if (!parseRecv(buffer, len, rec))
            return false;

        bool handled = true;
        if (!inPromiscuous)
        {

            handled = (_headerTo == 0xFF || _headerTo == terminalId);
        }
        if (handled)
        {
            if (rec.dv() == lastRcvMesssage)
            {
                return false;
            }
            else
            {
                lastRcvMesssage = rec.dv(); // elimina repetidos.
                if (!rxQueue.pushItem(rec))
                {

                    handled = false;
                }
            }
        }
        Logger::log(handled ? LogLevel::RECEIVE : LogLevel::WARNING, "(%d)[%X→%X:%X] L: %d Live: %d %s", _headerSender, _headerFrom, _headerTo, _headerId, len, _headerHope, buffer);
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
            sprintf(rec.value, "");
        }
        return true;
    }
};

LoRa32 lora;
#endif
