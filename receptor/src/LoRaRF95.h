#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
#endif
//--------------------------------------------------RF
SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX

class LoraRF
{
private:
    RH_RF95<decltype(RFSerial)> rf95;
    uint8_t terminalId = 1;
    bool _promiscuos = false;
    uint8_t _hto = 0xFF;
    uint8_t _hfrom = 0xFF;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';
    uint8_t sender = 0xFF;
    uint8_t nHeaderId = 0;

public:
    LoraRF() : rf95(RFSerial) {}
    bool begin(uint8_t terminal_Id, long band, bool promisc = true)
    {
        terminalId = terminal_Id;
        RFSerial.begin(Config::LORA_SPEED);
        bool result = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true); // ajusta situaçoes que não reconhece
        rf95.setFrequency(band);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        return result;
    }
    bool loop()
    {
        return true;
    }
    bool receive(char *buffer, uint8_t *len)
    {
        return receiveMessage(buffer, len);
    }

    void printRow(const char *msg)
    {
        Serial.println();
        Serial.print(msg);
        Serial.println("------------------------------------------------");
    }

    void ackIf(bool ack = false)
    {
        if (rf95.headerTo() == terminalId)
            send(rf95.headerFrom(), (ack) ? "ack" : "nak");
    }
    bool receiveMessage(char *buffer, uint8_t *len)
    {
        if (!rf95.waitAvailableTimeout(10))
            return false;

        uint8_t recvLen = Config::MESSAGE_MAX_LEN + 3;
        char localBuffer[recvLen] = {0};
        printRow("Entra RX");
        if (rf95.recv((uint8_t *)localBuffer, &recvLen))
        {
            localBuffer[recvLen] = '\0';
            // posicao 0 é o sender, retirar ele da mensagem
            if (recvLen < 3)
            {
                Logger::error("Mensagem muito curta");
                ackIf(false);
                return false;
            }
            if (localBuffer[1] != STX || localBuffer[recvLen - 1] != ETX)
            {
                Logger::error("Mensagem inválida ");
                Logger::error(localBuffer);
                ackIf(false);
                return false;
            }

#ifdef DEBUG_ON
            printHex(localBuffer, strlen(localBuffer));
#endif
            sender = localBuffer[0];

            if (sender == terminalId)
                return false;

            // copiar para message o byte 2 em diante e largar o ETX
            recvLen -= 3; // -2 para STX e ETX
            if (recvLen > Config::MESSAGE_MAX_LEN)
            {
                Logger::error("Mensagem muito longa");
                ackIf(false);
                return false;
            }
            strncpy(buffer, localBuffer + 2, recvLen);
            buffer[recvLen] = '\0'; // Garantir que a string esteja terminada
#ifdef DEBUG_ON
            printHex(buffer, recvLen);
#endif

            *len = recvLen;

            // Filtro de destino
            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            if (mfrom == terminalId)
                return false;

            if ((mto == terminalId) || (mto = 0xFF))
            {

                char fullLogMsg[64];

                snprintf(fullLogMsg, sizeof(fullLogMsg),
                         "[%d→%d] L:%d",
                         rf95.headerFrom(),
                         rf95.headerTo(),
                         recvLen);

                Logger::log(LogLevel::RECEIVE, fullLogMsg);
                Logger::log(LogLevel::RECEIVE, buffer);
            }
            //   printRow("Saindo RX");
#ifdef MESH
            uint8_t salto = rf95.headerFlags();
            if (salto > 1)
            {
                if (mto != terminalId)
                {
                    salto--;
                    send(rf95.headerTo(), buffer, rf95.headerFrom(), salto, rf95.headerId());
                }
            }
#endif
            return _promiscuos;
        }
        return false;
    }

    uint8_t headerTo()
    {
        return rf95.headerTo();
    }
    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool send(uint8_t tid, String buffer)
    {
        return send(tid, (char *)buffer.c_str(), buffer.length());
    }
    bool send(uint8_t tid, char *message, uint8_t from = 0xFF, uint8_t salto = 3, uint8_t seq = 0)
    {
        printRow("Entra TX");
        bool result = false;
        uint8_t len = strlen(message);
        uint8_t fromAjustado = (from == 0xFF) ? terminalId : from;
        rf95.setModeTx();
        rf95.setHeaderFrom(fromAjustado);
        rf95.setHeaderTo(tid);
        if (seq == 0)
            seq = ++nHeaderId;
        rf95.setHeaderId(seq);
        rf95.setHeaderFlags(salto, 0xFF);

        char fullMessage[Config::MESSAGE_MAX_LEN + 3];
        fullMessage[0] = terminalId;
        fullMessage[1] = STX;
        memcpy(fullMessage + 2, message, len);
        fullMessage[len + 2] = ETX;
        fullMessage[len + 3] = '\0';

        if (rf95.send((uint8_t *)fullMessage, strlen(fullMessage)))
        {
            if (rf95.waitPacketSent(2000))
            {

                char fullLogMsg[64];

                snprintf(fullLogMsg, sizeof(fullLogMsg),
                         "[%d-%d] L:%d",
                         fromAjustado,
                         tid,
                         len);
                Logger::log(LogLevel::SEND, fullLogMsg);
#ifdef _DEBUG_ON
                printHex(fullMessage, len + 2);
                // Logger::log(LogLevel::SEND, fullMessage);
#endif
            }
        };
        rf95.setModeRx();

        // printRow("Saindo TX");
        return result;
    }

    void printHex(char *fullMessage, uint8_t len)
    {

        Serial.println(fullMessage);
        Serial.print("HEX: ");
        Serial.print(len);
        Serial.print(" bytes: ");
        for (size_t i = 0; i < strlen(fullMessage); i++)
        {
            if (fullMessage[i] < 0x10)
                Serial.print('0'); // zero padding para valores < 0x10
            Serial.print(fullMessage[i], HEX);
            Serial.print(' ');
        }
        Serial.println("");
    }
    bool available()
    {
        return rf95.available();
    }
};
static LoraRF lora;

#endif