#ifndef RADIO_WIFI_ASYNC_H
#define RADIO_WIFI_ASYNC_H

#include "Arduino.h"
#include "RadioInterface.h"
#include <ESPAsyncTCP.h>
#include <vector>

struct Device
{
    uint8_t termId;
    AsyncClient *client;
    // Device() : id(0),
    //  client(nullptr){}
};

class RadioWiFi : public RadioInterface
{
private:
    AsyncServer *tcpServer;
    AsyncClient *tcpClient;
    std::vector<Device> clients; // Para modo gateway
    uint16_t tcpPort;
    unsigned long lastConnectionAttempt = 0;
    const unsigned long connectionInterval = 5000; // 5 segundos
    String pendingData;                            // Buffer para dados recebidos parcialmente
    bool isGateway;

public:
    RadioWiFi(uint16_t port = 12345, bool gatewayMode = false)
        : tcpPort(port), isGateway(gatewayMode), tcpServer(nullptr), tcpClient(nullptr) {}

    virtual ~RadioWiFi()
    {
        if (tcpServer)
        {
            tcpServer->end();
            delete tcpServer;
        }
        if (tcpClient)
        {
            tcpClient->close();
            delete tcpClient;
        }
        // bool removed = false;
        for (auto &dev : clients)
        {
            if (dev.client)
            {
                dev.client->close();
                delete dev.client;
                dev.client = nullptr;
                // removed = true;
                break;
            }
        }
    }

    virtual String getIdent() override
    {
        return "WiFiAsync";
    }

    virtual bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;
        _promiscuos = promisc;

        if (isGateway)
        {
            tcpServer = new AsyncServer(tcpPort);
            tcpServer->onClient([this](void *arg, AsyncClient *client)
                                { this->handleNewClient(client); }, nullptr);
            tcpServer->begin();
            connected = true;
            Serial.println("Modo Gateway WiFi (Async) iniciado");
        }
        else
        {
            // No modo cliente, a conexão será estabelecida no loop
            connected = false;
        }

        return true;
    }

    virtual void loop() override
    {
        // O AsyncTCP lida com os eventos em callbacks, então não precisamos fazer muito aqui
        if (!isGateway && !connected && millis() - lastConnectionAttempt > connectionInterval)
        {
            connectToGateway();
            lastConnectionAttempt = millis();
        }

        MessageRec txRec;
        if (txQueue.popItem(txRec))
        {
            sendMessage(txRec);
        }
    }

    virtual bool sendMessage(MessageRec &rec) override
    {
        if (isGateway)
        {
            // Serial.println("Enviando ");
            // rec.print();
            //  Envia para todos os clientes conectados
            bool sent = false;
            for (auto dev : clients)
            {
                if (dev.termId == rec.to || rec.to == 0xFF)
                {
                    if (dev.client && dev.client->connected())
                    {
                        sent |= sendToClient(dev.client, rec);
                    }
                }
            }
            return sent;
        }
        else
        {
            // Envia apenas para o gateway
            if (tcpClient && tcpClient->connected())
            {
                return sendToClient(tcpClient, rec);
            }
            return false;
        }
    }

    virtual bool receiveMessage() override
    {
        // As mensagens são processadas nos callbacks de recebimento
        return false;
    }

    virtual void modeRx() override
    {
        // Nada específico necessário para WiFi assíncrono
    }

    virtual void modeTx() override
    {
        // Nada específico necessário para WiFi assíncrono
    }

private:
    void handleNewClient(AsyncClient *client)
    {
        Serial.printf("Novo cliente conectado: %s\n", client->remoteIP().toString().c_str());

        // Configura callbacks para o novo cliente
        client->onData([this](void *arg, AsyncClient *c, void *data, size_t len)
                       { this->handleClientData(c, (char *)data, len); }, nullptr);

        client->onDisconnect([this](void *arg, AsyncClient *c)
                             { this->handleClientDisconnect(c); }, nullptr);

        client->onError([this](void *arg, AsyncClient *c, int8_t error)
                        { Serial.printf("Erro de conexão: %d\n", error); }, nullptr);

        client->onTimeout([this](void *arg, AsyncClient *c, uint32_t time)
                          { Serial.println("Timeout de conexão"); }, nullptr);

        Device newDev;
        newDev.client = client;
        newDev.termId = 0xFF;

        clients.push_back(newDev);

        // Envia identificação para o novo cliente
        MessageRec identMsg;
        identMsg.to = 0xFF; // Broadcast para o cliente
        identMsg.from = terminalId;
        identMsg.setEvent(EVT_PRESENTATION);
        identMsg.setValue(terminalName);
        identMsg.updateCRC();
        sendToClient(client, identMsg);
    }

    void handleClientData(AsyncClient *client, char *data, size_t len)
    {
        pendingData.concat(data, len);

        // Serial.println(pendingData.substring(6, pendingData.indexOf('\n')));
        // Serial.println(pendingData);
        // Serial.println(pendingData.indexOf('\n', 6)); // Processa todas as mensagens completas no buffer
        int endPos;
        while ((endPos = pendingData.indexOf('\n', 6)) != -1)
        {
            String message = pendingData.substring(0, endPos);
            // Serial.println(message);
            pendingData.remove(0, endPos + 1);

            MessageRec msg;
            if (msg.decode(message.c_str(), message.length()))
            {
                if (addRxMessage(msg))
                {
                    for (auto &dev : clients)
                    {
                        if (dev.client == client)
                        {
                            dev.termId = msg.from;
                        }
                    }
                }
                else
                    log(false, msg);
            }
            else
            {
                // msg.print();
            }
        }
    }

    void handleClientDisconnect(AsyncClient *client)
    {
        Serial.println("Cliente desconectado");
        for (auto it = clients.begin(); it != clients.end(); ++it)
        {
            if (it->client == client)
            {
                clients.erase(it);
                delete client;
                break;
            }
        }
    }
    int indexOf(const uint8_t termId)
    {
        for (size_t i = 0; i < clients.size(); i++)
            if (clients[i].termId == termId)
            {
                return i;
            }

        return -1;
    }
    bool sendToClient(AsyncClient *client, MessageRec &rec)
    {
        if (!client->connected())
        {
            return false;
        }
        // Serial.println();
        // Serial.print("sendToClient ");
        // Serial.print(rec.to);
        // Serial.print(" ");
        // Serial.println(client->localIP().toString());
        // rec.print();

        char buffer[MESSAGE_MAX_LEN];
        size_t len = rec.encode(buffer, sizeof(buffer));
        // Serial.print(len);
        // Serial.print(":");
        // Serial.println(buffer + 5);
        if (len == 0)
        {
            return false;
        }

        // Adiciona delimitador de fim de mensagem
        buffer[len++] = '\n';

        bool sent = client->write(buffer, len);
        if (sent)
        {
            log(true, rec);
        }
        return sent;
    }

    void connectToGateway()
    {
        if (tcpClient)
        {
            tcpClient->close();
            delete tcpClient;
        }

        tcpClient = new AsyncClient();

        // Configura callbacks para o cliente
        tcpClient->onConnect([this](void *arg, AsyncClient *c)
                             {
            Serial.println("Conectado ao gateway WiFi");
            connected = true;
            
            // Envia mensagem de identificação
            MessageRec identMsg;
            identMsg.to = 0xFF; // Broadcast para o gateway
            identMsg.from = terminalId;
            identMsg.setEvent("pub");
            identMsg.setValue(terminalName);
            identMsg.updateCRC();
            sendToClient(c, identMsg); }, nullptr);

        tcpClient->onData([this](void *arg, AsyncClient *c, void *data, size_t len)
                          { this->handleClientData(c, (char *)data, len); }, nullptr);

        tcpClient->onDisconnect([this](void *arg, AsyncClient *c)
                                {
            Serial.println("Desconectado do gateway");
            connected = false;
            delete c;
            tcpClient = nullptr; }, nullptr);

        tcpClient->onError([this](void *arg, AsyncClient *c, int8_t error)
                           {
            Serial.printf("Erro de conexão: %d\n", error);
            connected = false; }, nullptr);

        // Conecta ao gateway (substitua pelo IP do seu gateway)
        IPAddress gatewayIP(192, 168, 1, 1);
        tcpClient->connect(gatewayIP, tcpPort);
    }
};

#endif // RADIO_WIFI_ASYNC_H