#ifndef LORA32_H
#define LORA32_H

#include "Arduino.h"
#include "queue_message.h"
#include "config.h"
#include "logger.h"
#include "LoRaInterface.h"

#ifdef ESP32

// HELTEC
#if defined(HELTEC)
#include "Arduino.h"
#include "heltec.h"
#ifdef Heltec_LoRa
#define LoRa Heltec.LoRa
#else

#include "lora/LoRa.h"
#endif

#else
// TTGO
#include "LoRa.h"
#endif
#endif

// Constantes para controle de mesh
#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

//==========================================================

class LoRa32 : public LoRaInterface
{
private:
    bool isHeltec = false;

public:
    bool connected = false;
    void modeTx() override
    {
        LoRa.idle();
    }
    void modeRx() override
    {
        LoRa.receive();
    }

    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        _promiscuos = promisc;
        terminalId = terminal_Id;

#ifdef HELTEC
        isHeltec = true;
        Heltec.begin(Heltec_Screen /*DisplayEnable*/, false /*LoRaEnable*/, false /*SerialEnable*/);
        delay(100);

        // LoRa.setPins(Config::LORA_CS, Config::LORA_RST, Config::LORA_IRQ);
        if (LoRa.begin(band, true))
        {
            connected = true;
        }; // true /* PABOOST */);

        // Configurações otimizadas para Heltec
        LoRa.setSpreadingFactor(7);
        LoRa.setSignalBandwidth(125E3);
        LoRa.setCodingRate4(5);
        LoRa.setSyncWord(Config::LORA_SYNC_WORD);

        // LoRa.setTxPower(14, RF_PACONFIG_PASELECT_RFO); // Usar RFO para menor potência
        //  Ou alternativamente:
        LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST); // Máxima potência
        LoRa.setPreambleLength(8);

        // Habilitar CRC
        LoRa.enableCrc();
        LoRa.setTxPowerMax(14);
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

    void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) override
    {
        LoRa.setPins(cs, reset, irq);
    }

    uint8_t _headerTo;
    uint8_t _headerFrom;
    uint8_t _headerId;
    uint8_t _headerSender;
    uint8_t _headerHope;
    long lastRcvMesssage;

    bool sendMessage(MessageRec &rec) override
    {
        return sendMessage(rec.to, rec.event, rec.value, rec.from, rec.hope, rec.id);
    }

    // Modificar o método sendMessage para o Heltec
    bool sendMessage(uint8_t tidTo, const char *event, const char *value, const uint8_t tidFrom, uint8_t hope, uint8_t id)
    {
        char message[Config::MESSAGE_MAX_LEN] = {0};
        int x = snprintf(message, sizeof(message), "{%s|%s}", event, value);
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
        // LoRa.write('{');
        // LoRa.write((const uint8_t *)message, len);
        LoRa.print(message);
        // LoRa.write('}');

        int result = 0;

#ifdef HELTEC
        // Configuração específica para Heltec
        result = LoRa.endPacket(true); // true = async mode
#else
        result = LoRa.endPacket(true);
#endif

        if (result > 0)
        {
            Logger::log(LogLevel::SEND, "(%d)[%X→%X:%X](%d) %s",
                        terminalId, tidFrom, tidTo, id, hope, message);

            // Espera adicional apenas para Heltec
            delay(10); // Pequeno delay para garantir o envio

            return true;
        }
        else
        {
            Logger::error("Failed to send packet");
            return false;
        }
    }

    bool receiveLogger(const bool result, LogLevel level, const char *buffer) const
    {
        Logger::log(level,
                    "(%d)[%X→%X:%X](%d) L: %d  %s",
                    _headerSender, _headerFrom, _headerTo, _headerId, _headerHope, strlen(buffer), buffer);
        return result;
    }

    bool receiveMessage()
    {

        if (!LoRa.available())
            return false;

        uint8_t packetSize = LoRa.parsePacket();
        if (packetSize < 5)
            return false;
        delay(5);
        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t len = 0;

        _headerTo = LoRa.read();
        _headerFrom = LoRa.read();
        _headerId = LoRa.read();
        _headerHope = LoRa.read();
        _headerSender = LoRa.read();

        memset(buffer, 0, sizeof(buffer));
        bool passouPipe = false;

        if (isHeltec)
            LoRa.setTimeout(10);
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

        buffer[len] = '\0';

        MessageRec rec;
        if (!parseRecv(buffer, len, rec))
            return false;

        //        receiveLogger(false, LogLevel::VERBOSE, buffer);

        if (len == 0 || _headerFrom == terminalId || _headerSender == terminalId)
            return false;

        // Tratamento de mensagens de controle (ping/pong)
        if (strstr(buffer, "ping") != NULL)
        {
            if (_headerTo != terminalId)
                return false;
            txQueue.push(_headerFrom, "pong", terminalName, terminalId, ALIVE_PACKET, _headerId);
            return true;
        }
        else if (strstr(buffer, "pong") != NULL)
        {
            if (!isDeviceActive(_headerFrom))
            {
                Logger::log(LogLevel::INFO, "Pong received from: %d", _headerFrom);
                // setDeviceActive(_headerFrom);
            }
            return false;
        }

        bool handled = (_headerTo == 0xFF || _headerTo == terminalId);

        if (_headerTo == 0xFF || _headerTo != terminalId)
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
                if (salto > 0 && salto <= ALIVE_PACKET && terminalId != 0)
                {
                    if (_headerTo != terminalId && !isDeviceActive(_headerFrom))
                    {
                        salto--;
                        rec.hope = salto;
                        txQueue.pushItem(rec);
                        Logger::log(LogLevel::INFO, "MESH From: %d", _headerFrom);
                    }
                }
            }
        }

        if (handled)
        {
            if (!rxQueue.pushItem(rec))
            {
                handled = false;
            }
            Logger::log(handled ? LogLevel::RECEIVE : LogLevel::WARNING,
                        "(%d)[%X→%X:%X](%d) L: %d  %s",
                        _headerSender, _headerFrom, _headerTo, _headerId, _headerHope, len, buffer);
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
    int packetSnr() override
    {
        return LoRa.packetSnr();
    }
};

static LoRa32 lora;
#endif