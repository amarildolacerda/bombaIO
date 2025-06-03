#ifndef LORA_INTERFACE_H
#define LORA_INTERFACE_H

#include <Arduino.h>
#include <queue_message.h>

//[stable]
enum LoRaStates
{
    LoRaTX,
    LoRaRX,
    LoRaWAITING,
    LoRaIDLE,
};

// Constantes para controle de mesh
#define ALIVE_PACKET 3
#define MAX_MESH_DEVICES 255
#define MESSAGE_TIMEOUT_MS 2000

class LoRaInterface
{
protected:
    uint8_t terminalId = 0;
    char terminalName[10] = {0};
    FifoList txQueue;
    FifoList rxQueue;
    long rxDelay = 0;
    long txDelay = 0;

    uint8_t nHeaderId = 0;
    uint8_t headerSender = 0;
    bool _promiscuos = false;

    // Controle de dispositivos ativos na mesh
    uint8_t noMesh[(MAX_MESH_DEVICES + 7) / 8] = {0};

    const char bEOF = '}';
    const char bBOF = '{';
    const char bSEP = '|';
    LoRaStates state = LoRaRX;

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

    bool isValidMessage(const char *msg, uint8_t len)
    {
        return (len >= 4) && (msg[1] == '{') && (msg[len - 1] == '}');
    }

public:
    bool connected = false;

    virtual bool begin(const uint8_t terminal_Id, long band, bool promisc = true) = 0;
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

    virtual ~LoRaInterface() {}
    virtual bool send(uint8_t tid, const char *event, const char *value, const uint8_t terminalId)
    {
        MessageRec rec;
        rec.to = tid;
        rec.from = terminalId;
        snprintf(rec.event, sizeof(rec.event) - 1, event);
        snprintf(rec.value, sizeof(rec.value) - 1, value);
        rec.hope = ALIVE_PACKET;
        rec.id = ++nHeaderId;
        return txQueue.pushItem(rec);
    }
    virtual bool processIncoming(MessageRec &rec)
    {
        bool result = false;
        result = rxQueue.popItem(rec);
        return result;
    }
    virtual void setState(LoRaStates st) = 0;

    // Modificar o loop para tratamento específico do Heltec
    virtual bool loop()
    {
        bool handled = false;
        static unsigned long lastTxCheck = 0;
        const unsigned long txCheckInterval = 100; // Verificar TX a cada 100ms
        static unsigned long txStartTime = 0;

        switch (state)
        {
        case LoRaIDLE:
            // Estado inicial - decidir para onde ir
            if (txQueue.size() > 0)
            {
                setState(LoRaTX);
            }
            else
            {
                setState(LoRaRX);
            }
            break;

        case LoRaRX:
            // 1. Primeiro processar recebimento
            if (millis() - rxDelay > 10)
            { // Processar RX a cada 10ms
                rxDelay = millis();
                handled = receiveMessage();
            }

            // 2. Verificar se precisa transmitir periodicamente
            if (millis() - lastTxCheck > txCheckInterval)
            {
                lastTxCheck = millis();

                if (txQueue.size() > 0)
                {
                    setState(LoRaTX);
                    txStartTime = millis();
                }
            }
            break;

        case LoRaTX:
            // Processar transmissão
            if (millis() - txDelay > 50)
            { // Espaço entre transmissões
                txDelay = millis();
                MessageRec rec;

                if (txQueue.popItem(rec))
                {
                    if (sendMessage(rec))
                    {
                        handled = true;
                    }
                    else
                    {
                        // Se falhar, recoloca na fila (no final)
                        txQueue.pushItem(rec);
                    }
                }

                // Decidir próximo estado
                if (txQueue.size() > 0)
                {
                    // Continua em TX se houver mais mensagens
                    txDelay = millis();
                }
                else
                {
                    // Volta para RX quando fila estiver vazia
                    setState(LoRaRX);
                }
                // Dentro do case LoRaTX:
                if (millis() - txStartTime > 100)
                {
                    // Se ficar muito tempo em TX, volta para RX
                    setState(LoRaRX);
                }
            }
            break;

        default:
            setState(LoRaRX);
            break;
        }

        return handled;
    }
    virtual bool sendMessage(MessageRec &rec) = 0;
    virtual bool receiveMessage() = 0;
};

#endif // LORA_INTERFACE_H
