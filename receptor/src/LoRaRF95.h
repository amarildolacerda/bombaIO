#ifndef LORARF95_H
#define LORARF95_H

#include "RH_RF95.h"
#include "config.h"
#include "logger.h"

#ifdef __AVR__
#include <SoftwareSerial.h>
#endif

#define ALIVE_PACKET 3

//--------------------------------------------------RF
SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX

struct meshrec
{
    bool active = false;
    uint8_t to;
    uint8_t from;
    char msg[128];
    uint8_t live;
    uint8_t id;
};
class LoraRF
{
private:
    meshrec mesh;
    RH_RF95<decltype(RFSerial)> rf95;
    uint8_t terminalId = 1;
    bool _promiscuos = false;
    uint8_t _hto = 0xFF;
    uint8_t _hfrom = 0xFF;
    const uint8_t STX = '{';
    const uint8_t ETX = '}';
    uint8_t sender = 0xFF;
    uint8_t nHeaderId = 0;
    // const char *terminalName = Config::TERMINAL_NAME;

public:
    bool connected = false;
    LoraRF() : rf95(RFSerial) {}
    bool begin(uint8_t terminal_Id, long band, bool promisc = true)
    {

        terminalId = Config::TERMINAL_ID;
        RFSerial.begin(Config::LORA_SPEED);
        connected = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true); // ajusta situaçoes que não reconhece
        rf95.setFrequency(band);
        rf95.setHeaderFlags(0, RH_FLAGS_NONE);
        rf95.setTxPower(14, false);
        return connected;
    }
    bool loop()
    {
        if (mesh.active)
        {
            delay(100);
            mesh.active = false;
            Serial.print("MESH ");
            send(mesh.to, mesh.msg, mesh.from, mesh.live, mesh.id);
        }
        return true;
    }
    bool receive(char *buffer, uint8_t *len)
    {
        return receiveMessage(buffer, len);
    }

    void printRow(const char *msg)
    {
#ifdef DEBUG_ON
        Serial.println();
        Serial.print(msg);
        Serial.println("------------------------------------------------");
#endif
    }

    void ackIf(bool ack = false)
    {
        if (rf95.headerTo() == terminalId)
            send(rf95.headerFrom(), (ack) ? "ack" : "nak");
    }
    bool noMesh[255];
    bool receiveMessage(char *buffer, uint8_t *len)
    {
        if (!rf95.waitAvailableTimeout(10))
            return false;

#ifdef DEBUG_ON
        Serial.print("RX ");
#endif
        uint8_t recvLen = Config::MESSAGE_MAX_LEN + 3;
        char localBuffer[recvLen] = {0};
        if (rf95.recv((uint8_t *)localBuffer, &recvLen))
        {
            localBuffer[recvLen] = '\0';
            String sResult = String(localBuffer).substring(2, recvLen - 1);
            // posicao 0 é o sender, retirar ele da mensagem
            if (recvLen < 3)
            {
                Logger::error((char *)"Mensagem muito curta");
                ackIf(false);
                return false;
            }
            if (localBuffer[1] != STX || localBuffer[recvLen - 1] != ETX)
            {
                Logger::error("Mensagem inválida ");
                // Logger::error(localBuffer);
                ackIf(false);
                return false;
            }

            uint8_t mto = rf95.headerTo();
            uint8_t mfrom = rf95.headerFrom();
            sender = (uint8_t)localBuffer[0];

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

            // Filtro de destino
            if (mfrom == terminalId)
                return false;

            Serial.println(sResult);
            if (sResult.indexOf("ping") >= 0)
            {
                // se nao for meu não deve responder
                if (mto != terminalId)
                    return false;
                // String sCmd = "pong|0" + (String)terminalName;
                send(0, (char *)"pong|0");
                return true;
            }
            if (sResult.indexOf("pong") >= 0)
            {
                // nao prescisa enviar mesh
                noMesh[mfrom] = true;
                return false;
            }

            *len = sResult.length();
            strncpy(buffer, sResult.c_str(), *len);
            buffer[*len] = '\0'; // Garantir que a string esteja terminada

            if ((mto == terminalId) || (mto = 0xFF))
            {
#ifdef DEBUG_ON
                // printHex(localBuffer, strlen(localBuffer));
                /* char fullLogMsg[128] = {0};
                 snprintf(fullLogMsg, sizeof(fullLogMsg),
                          "(%d)[%d→%d] L:%d",
                          sender,
                          rf95.headerFrom(),
                          rf95.headerTo(),
                          recvLen);
                 Logger::log(LogLevel::RECEIVE, fullLogMsg);
                 */
                Serial.print("\033[32m");

                Serial.print("[SEND]-[");
                Serial.print(mfrom);
                Serial.print("-");
                Serial.print(mto);
                Serial.print("] ");
                Serial.print(recvLen);
                Serial.print("\033[0m"); // Reset color if used
                Serial.println();

                // Serial.println(buffer);
#endif
                if (mto == terminalId)
                    return true;
            }
            //   printRow("Saindo RX");
#ifdef MESH
            uint8_t salto = rf95.headerFlags();
            if (salto > 1 && salto <= ALIVE_PACKET)
            {
                if (mto != terminalId && !noMesh[mesh.from])
                {
                    salto--;
                    // Logger::info(String("MESH to: " + String(rf95.headerTo()) + " live: " + String(salto)).c_str());
                    mesh.to = rf95.headerTo();
                    mesh.from = rf95.headerFrom();
                    mesh.id = rf95.headerFlags();
                    memset(mesh.msg, 0, sizeof(mesh.msg));
                    strncpy(mesh.msg, sResult.c_str(), *len);
                    mesh.live = salto;
                    mesh.active = strlen(mesh.msg) > 2;
                }
            }
#endif
            return _promiscuos || (mto == 0xFF);
        }
        return true;
    }

    uint8_t headerTo()
    {
        return rf95.headerTo();
    }
    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool send(uint8_t terminalTo, String buffer)
    {
        return send(terminalTo, (char *)buffer.c_str(), buffer.length());
    }
    bool send(uint8_t terminalTo, char *message, uint8_t terminalFrom = Config::TERMINAL_ID, uint8_t salto = ALIVE_PACKET, uint8_t seq = 0)
    {

#ifdef DEBUG_ON
        Serial.print("TX ");
#endif
        bool result = false;
        uint8_t len = strlen(message);

        rf95.setModeTx();
        rf95.setHeaderFrom(terminalFrom);
        rf95.setHeaderTo(terminalTo);
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
                Serial.print("[");
                Serial.print(terminalFrom);
                Serial.print("-");
                Serial.print(terminalTo);
                Serial.print("] ");
                Logger::log(LogLevel::SEND, message);
#ifdef DEBUG_ON
                // printHex(fullMessage, len + 2);
                // Serial.print(fullMessage);
                char fullLogMsg[64];
                snprintf(fullLogMsg, sizeof(fullLogMsg),
                         "(%d)[%d-%d] L:%d",
                         terminalId,
                         terminalFrom,
                         terminalTo,
                         len);
                Logger::log(LogLevel::SEND, fullLogMsg);
#endif
            }
        };
        rf95.setModeRx();

        // printRow("Saindo TX");
        return result;
    }

    void printHex(char *fullMessage, uint8_t len)
    {
#ifdef xDEBUG_ON
        // Serial.println(fullMessage);
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

#endif
    }
    bool available()
    {
        return rf95.waitAvailableTimeout(10);
    }
};
static LoraRF lora;

#endif