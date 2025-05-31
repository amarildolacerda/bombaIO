#ifndef LORA32_H
#define LORA32_H

#include "Arduino.h"
#include "queue_message.h"
#include "config.h"
#include "logger.h"
#include "LoRaInterface.h"

#ifdef ESP32
#if defined(HELTEC)
#include "Arduino.h"
#include "heltec.h"
#include "lora/LoRa.h"
#else
#include "LoRa.h"
#endif
#endif

// Constantes para controle de mesh
#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

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
    bool isHeltec = false;

    bool inPromiscuous = true;

    // Controle de dispositivos ativos na mesh
    uint8_t noMesh[(MAX_MESH_DEVICES + 7) / 8] = {0};

    const char bEOF = '}';
    const char bBOF = '{';
    const char bSEP = '|';
    LoRaStates state = LoRaRX;
    uint8_t terminalId = 0;

    // Funções para controle de dispositivos ativos
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
            break;
        case LoRaTX:
            LoRa.idle();
            break;
        case LoRaWAITING:
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
        terminalId = terminal_Id;

#ifdef HELTEC
        isHeltec = true;
        Heltec.begin(true /*DisplayEnable*/, false /*LoRaEnable*/, true /*SerialEnable*/);
        // LoRa.setPins(Config::LORA_CS, Config::LORA_RST, Config::LORA_IRQ);
        LoRa.begin(band, true /* PABOOST */);

        // Configurações otimizadas para Heltec
        LoRa.setSpreadingFactor(7);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setCodingRate4(5);
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);

        //        LoRa.setTxPower(14, RF_PACONFIG_PASELECT_RFO); // Usar RFO para menor potência
        // Ou alternativamente:
        LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST); // Máxima potência
        LoRa.setPreambleLength(8);

        // Habilitar CRC
        LoRa.enableCrc();
        LoRa.setTxPowerMax(20);
#else
        isHeltec = false;
        // LoRa.setPins(Config::LORA_CS, Config::LORA_RST, Config::LORA_IRQ);
        LoRa.begin(band);
        LoRa.setSpreadingFactor(7);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setCodingRate4(5);
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

    // Modificar o loop para tratamento específico do Heltec
    bool loop() override
    {
        bool handled = false;

        switch (state)
        {
        case LoRaRX:
            if (LoRa.available() && millis() - rxDelay > 5)
            {
                rxDelay = millis();
                handled = receiveMessage();
            }
            if (txQueue.size() > 0)
            {
                setState(LoRaTX);
            }
            break;

        case LoRaTX:
            if (millis() - txDelay > (isHeltec ? 20 : 5))
            { // Delay maior para Heltec
                txDelay = millis();
                MessageRec txRec;
                if (txQueue.pop(txRec))
                {
                    handled = sendMessage(txRec.to, txRec.event, txRec.value,
                                          txRec.from, txRec.hope, txRec.id);
                }
            }
            break;

        case LoRaWAITING:
            // Estado não é mais necessário com a implementação modificada
            setState(LoRaRX);
            break;

        default:
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
        rec.hope = ALIVE_PACKET;
        rec.id = ++seq;
        return txQueue.pushItem(rec);
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
    long lastRcvMesssage;

    // Modificar o método sendMessage para o Heltec
    bool sendMessage(uint8_t tidTo, const char *event, const char *value, const uint8_t tidFrom, uint8_t hope, uint8_t id)
    {
        char message[Config::MESSAGE_MAX_LEN] = {0};
        int x = snprintf(message, sizeof(message), "%s|%s", event, value);
        message[x] = '\0';

        uint8_t len = strlen(message);
        if (len == 0 || len > Config::MESSAGE_MAX_LEN - 3)
        {
            Logger::error("Message size invalid");
            return false;
        }

        LoRa.idle();
        LoRa.beginPacket();

        // Escrever cabeçalho
        LoRa.write(tidTo);
        LoRa.write(tidFrom);
        LoRa.write(id);
        LoRa.write(hope);
        LoRa.write(terminalId);

        // Escrever payload
        LoRa.write('{');
        LoRa.write((const uint8_t *)message, len);
        LoRa.write('}');

        int result = 0;

#ifdef HELTEC
        // Configuração específica para Heltec
        result = LoRa.endPacket(true); // true = async mode
#else
        result = LoRa.endPacket();
#endif

        if (result > 0)
        {
            Logger::log(LogLevel::SEND, "(%d)[%X→%X:%X](%d) %s",
                        terminalId, tidFrom, tidTo, id, hope, message);

            // Espera adicional apenas para Heltec
            if (isHeltec)
            {
                delay(10); // Pequeno delay para garantir o envio
            }

            setState(LoRaRX);
            return true;
        }
        else
        {
            Logger::error("Failed to send packet");
            setState(LoRaRX);
            return false;
        }
    }

    bool receiveMessage()
    {
        if (!LoRa.available())
            return false;

        uint8_t packetSize = LoRa.parsePacket();
        if (packetSize < 5)
            return false;

        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t len = 0;

        _headerTo = LoRa.read();
        _headerFrom = LoRa.read();
        _headerId = LoRa.read();
        _headerHope = LoRa.read();
        _headerSender = LoRa.read();

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

        // Tratamento de mensagens de controle (ping/pong)
        if (strstr(buffer, "ping") != NULL)
        {
            if (_headerTo != terminalId)
                return false;
            txQueue.push(_headerFrom, "pong", "0", terminalId, ALIVE_PACKET, _headerId);
            return true;
        }
        else if (strstr(buffer, "pong") != NULL)
        {
            if (!isDeviceActive(_headerFrom))
            {
                Logger::log(LogLevel::INFO, "Pong received from: %d", _headerFrom);
                setDeviceActive(_headerFrom);
            }
            return false;
        }

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
                lastRcvMesssage = rec.dv();

                // Implementação do mesh - retransmissão de mensagens
                // quanto terminalId é zero, o pacote ja chegou, não é para fazer mesh
                // os dispositivos roteiam entre si para tentar chegar no gateway
                // uma vez chegou no gateway, fim de transmissão
                uint8_t salto = _headerHope;
                if (salto > 1 && salto <= ALIVE_PACKET && terminalId != 0)
                {
                    if (_headerTo != terminalId && !isDeviceActive(_headerFrom))
                    {
                        salto--;
                        rec.hope = salto;
                        txQueue.pushItem(rec);
                        Logger::log(LogLevel::INFO, "MESH From: %d", _headerFrom);
                    }
                    return _headerTo == 0xFF;
                }

                if (!rxQueue.pushItem(rec))
                {
                    handled = false;
                }
            }
        }

        Logger::log(handled ? LogLevel::RECEIVE : LogLevel::WARNING,
                    "(%d)[%X→%X:%X] L: %d Live: %d %s",
                    _headerSender, _headerFrom, _headerTo, _headerId, len, _headerHope, buffer);
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
    int packetRssi() override
    {
        return LoRa.packetRssi();
    }
};

LoRa32 lora;
#endif