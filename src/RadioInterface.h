#ifndef LORA_INTERFACE_H
#define LORA_INTERFACE_H

#include <Arduino.h>
#include <queue_message.h>
#include "logger.h"

#define MESSAGE_MAX_LEN 128

//[stable]
enum RadioStates
{
    RadioStateRX,
    RadioStateTX
};

// Constantes para controle de mesh
#define ALIVE_PACKET 3
#define MESSAGE_TIMEOUT_MS 10

class RadioInterface
{
protected:
    uint8_t terminalId = 0;
    char terminalName[10] = {0};
    FifoList txQueue;
    FifoList rxQueue;
    unsigned long lastStateChange = millis();

    uint8_t nHeaderId = 0;
    uint8_t headerSender = 0;
    bool _promiscuos = false;

    const char bEOF = '}';
    const char bBOF = '{';
    const char bSEP = '|';
    RadioStates state = RadioStateRX;
    bool connected = false;

private:
    void setState(RadioStates st)
    {
        state = st;
        switch (state)
        {
        case RadioStateRX:
            modeRx();
            /* code */
            break;
        case RadioStateTX:
            modeTx();
            break;
        default:

            break;
        }
    }

public:
    bool isConnected()
    {
        return connected;
    }
    bool isValidMessage(const char *msg, uint8_t len)
    {
        return (len >= 4) && (msg[0] == '{') && (msg[len - 1] == '}');
    }

    void meshMessage(MessageRec &rec)
    {
        if (rec.to != terminalId && terminalId > 0) // avaliar mesh
        {                                           // o gateway nao retransmite, para o caso de ser um terminal
            uint8_t hope = rec.hope;
            if (hope > 0)
            {
                rec.hope--;
                txQueue.pushItem(rec); // re-enviar a mensagem para o gateway
            }
        }
    }
    bool parseRcv(MessageRec &rec, const String msg)
    {

        if (!msg.startsWith("{") || !msg.endsWith("}"))
        {
            Logger::error("Mensagem mal formatada: %s", msg);
            return false;
        }

        String content = msg.substring(1, msg.length() - 1); // remove { and }
        int sepIndex = content.indexOf('|');
        sprintf(rec.event, "%s", content.substring(0, sepIndex).c_str());
        if (sepIndex != -1)
        {
            sprintf(rec.value, "%s", content.substring(sepIndex + 1).c_str());
        }
        else
        {
            rec.value[0] = '\0'; // no value provided
        }

        return true;
    }

    virtual void setTerminalName(const char name[10])
    {
        snprintf(terminalName, sizeof(terminalName), name);
    };
    virtual void setTerminalId(uint8_t tid)
    {
        terminalId = tid;
    };
    virtual void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) {};
    virtual void endSetup()
    {
    }
    virtual int packetRssi()
    {
        return 0;
    };
    virtual int packetSnr()
    {
        return 0;
    }
    virtual bool available()
    {
        return !rxQueue.isEmpty();
    }

    virtual ~RadioInterface() {}
    virtual bool receive(MessageRec &rec)
    {
        if (rec.id == 0)
            rec.id = ++nHeaderId;
        return rxQueue.pushItem(rec);
    }

    virtual bool
    send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId)
    {
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = tid;
        rec.from = terminalId;
        snprintf(rec.event, sizeof(rec.event), event);
        snprintf(rec.value, sizeof(rec.value), value);
        rec.hope = ALIVE_PACKET;
        rec.id = ++nHeaderId;
        return txQueue.pushItem(rec);
    }
    virtual bool processIncoming(MessageRec &rec)
    {
        return rxQueue.popItem(rec);
    }
    bool sendNextMessage()
    {
        MessageRec rec;
        if (!txQueue.popItem(rec))
        {
            // Serial.println("No message to send");
            return false;
        }
        return sendMessage(rec);
    }

    // Modificar o loop para tratamento específico do Heltec
    virtual void loop()
    {
        switch (state)
        {
        case RadioStateRX:
            if (receiveMessage())
                lastStateChange = millis();

            // Reduzido de 100ms para 50ms para melhorar throughput
            if (txQueue.size() > 0 && millis() - lastStateChange > MESSAGE_TIMEOUT_MS)
            {
                setState(RadioStateTX);
                lastStateChange = millis();
            }
            break;

        case RadioStateTX:
            if (sendNextMessage())
            {
                lastStateChange = millis();
            }

            // Reduzido de 100ms para 50ms
            if (millis() - lastStateChange > MESSAGE_TIMEOUT_MS)
            {
                setState(RadioStateRX);
                lastStateChange = millis();
            }

            break;
        }
    }
    virtual void modeRx() {};
    virtual void modeTx() {};
    virtual bool sendMessage(MessageRec &rec) = 0;
    virtual bool receiveMessage() = 0;
    virtual bool begin(const uint8_t terminal_Id, long band, bool promisc = true) = 0;
};

#endif // LORA_INTERFACE_H
