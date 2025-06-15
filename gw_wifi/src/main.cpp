#include <ESP8266WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncTCP.h>
#include <ESP8266HTTPClient.h>
#include "queue_message.h"
#include "event_types.h"
#include "message_router.h"

// Configurações
#define WIFI_SSID "sua_rede"
#define WIFI_PASS "sua_senha"
#define TCP_PORT 12345
#define MAX_DEVICES 8

AsyncServer tcpServer(TCP_PORT);
MessageRouter router;
FifoList txQueue(20);
FifoList rxQueue(20);

struct Device
{
    AsyncClient *client;
    uint8_t id;
    String name;
};

Device devices[MAX_DEVICES];

void setup()
{
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);

    // Configura roteador
    router.registerHandler(MSG_TYPE_CMD, handleCommand);
    router.registerHandler(MSG_TYPE_EVT, handleEvent);
    router.registerHandler(MSG_TYPE_ACT, handleActuator);

    // Configura servidor
    tcpServer.onClient([](void *arg, AsyncClient *client)
                       { handleNewClient(client); }, nullptr);
    tcpServer.begin();
}

void loop()
{
    processMessages();
    delay(10);
}

void handleNewClient(AsyncClient *client)
{
    for (auto &dev : devices)
    {
        if (!dev.client)
        {
            dev.client = client;
            client->onData([](void *arg, AsyncClient *c, void *data, size_t len)
                           { processIncomingData(c, data, len); }, nullptr);
            // ... outros callbacks
            break;
        }
    }
}

void processIncomingData(AsyncClient *client, void *data, size_t len)
{
    MessageRec msg;
    if (msg.decode((char *)data, len) && msg.verifyCRC())
    {
        router.routeMessage(msg);
    }
}

void processMessages()
{
    MessageRec msg;
    while (rxQueue.popItem(msg))
    {
        router.routeMessage(msg);
    }
}

// Handlers específicos
void handleCommand(MessageRec &msg)
{
    if (msg.isEvent(CMD_IDENTIFY))
    {
        // Processa identificação
        registerDevice(msg);
    }
    // ... outros comandos
}

void handleEvent(MessageRec &msg)
{
    if (msg.isEvent(EVT_TEMPERATURE))
    {
        sendToCloud(msg);
    }
    // ... outros eventos
}

void handleActuator(MessageRec &msg)
{
    if (msg.isEvent(ACT_RELAY))
    {
        controlRelay(msg.value);
    }
    // ... outros atuadores
}

// Funções auxiliares
void registerDevice(MessageRec &msg)
{
    for (auto &dev : devices)
    {
        if (dev.client && dev.client->remoteIP() == msg.fromIP)
        {
            dev.id = msg.from;
            dev.name = msg.value;
            sendAck(dev.client);
            break;
        }
    }
}

void sendToCloud(MessageRec &msg)
{
    // Implementação do envio HTTP
}

void controlRelay(const char *state)
{
    // Implementação do controle
}