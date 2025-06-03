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
#ifdef DEBUG_ON
            Serial.println("LoRaRX");
#endif
            rf95.setModeRx();
            rxDelay = millis();
            break;
        case LoRaTX:
#ifdef DEBUG_ON
            Serial.println("LoRaTX");
#endif
            rf95.setModeTx();
            txDelay = millis();
            break;
        case LoRaWAITING:
#ifdef DEBUG_ON
            Serial.println("LoRaWAITING");
#endif
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
            rec.value[0] = '\0';
            // sprintf(rec.value, '\0');
        }
        return true;
    }

    bool receiveMessage() override
    {
        if (!rf95.waitAvailableTimeout(50))
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
#ifdef DEBUG_ON
                        Serial.print("MESH From: ");
                        Serial.println(rf95.headerFrom());
#endif
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
    {
#ifdef DEBUG_ON
        Serial.println("\n--- Iniciando sendMessage ---");
        Serial.print("Conteúdo: ");
        rec.print();
#endif

        // 1. Preparar mensagem
        char message[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "%s|%s", rec.event, rec.value);

        if (len == 0 || len > Config::MESSAGE_MAX_LEN)
        {
#ifdef DEBUG_ON
            Serial.println("ERRO: Tamanho de mensagem inválido");
#endif
            return false;
        }

        // 2. Configurar rádio
        rf95.setModeTx();

        rf95.setHeaderFrom(rec.from);
        rf95.setHeaderTo(rec.to);
        rf95.setHeaderId(rec.id);
        rf95.setHeaderFlags(rec.hope, 0xFF);

        // 3. Formatar mensagem final
        char fullMessage[Config::MESSAGE_MAX_LEN];
        snprintf(fullMessage, sizeof(fullMessage), "%c{%s}", terminalId, message);
        len += 3; // Adiciona tamanho dos delimitadores

#ifdef DEBUG_ON
        Serial.print("Enviando: ");
        Serial.println(fullMessage);
#endif
        // 4. Tentar enviar
        bool sendResult = false;
        for (int attempt = 0; attempt < 3 && !sendResult; attempt++)
        {
#ifdef DEBUG_ON
            Serial.print("Tentativa ");
            Serial.println(attempt + 1);
#endif
            if (attempt > 0)
                delay(100 * attempt); // Backoff

            if (rf95.send((uint8_t *)fullMessage, len))
            {
#ifdef DEBUG_ON
                Serial.println("send() ok, aguardando confirmação...");
#endif
                if (rf95.waitPacketSent(5000))
                { // Timeout de 5 segundos
                    sendResult = true;
#ifdef DEBUG_ON
                    Serial.println("Mensagem enviada com sucesso!");
#endif
                    Logger::log(LogLevel::SEND, "(%d)[%X->%X:%X](%d) %s", terminalId, rec.from,
                                rec.to, rec.id, rec.hope, fullMessage);
                }
                else
                {
                    Serial.println("ERRO: Timeout no waitPacketSent");
                }
            }
            else
            {
                Serial.println("ERRO: Falha no send()");
            }
        }

#ifdef DEBUG_ON
        Serial.println("--- Finalizando sendMessage ---");
#endif
        return sendResult;
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