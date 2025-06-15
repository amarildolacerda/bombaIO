#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266HTTPClient.h>
#include "queue_message.h"
#include "event_types.h"
#include "message_router.h"
#include "logger.h"

#include "ESPAsyncWiFiManager.h"
#include "RadioWiFi.h"

RadioWiFi radio(12345, true);

AsyncWebServer server(80);
DNSServer dns;

AsyncWiFiManager wm(&server, &dns);

// Configurações
#define WIFI_SSID "kcasa"
#define WIFI_PASS "3938373635"
#define TCP_PORT 12345
#define MAX_DEVICES 8

// AsyncServer tcpServer(TCP_PORT);
// MessageRouter router;
// FifoList txQueue(20);
// FifoList rxQueue(20);

class App
{

public:
    // void handleCommand(MessageRec &msg);
    // void processMessages();
    // void handleEvent(MessageRec &msg);
    // void handleNewClient(AsyncClient *client);
    // void receiveMessage(AsyncClient *client, void *data, size_t len);
    // void registerDevice(AsyncClient *client, MessageRec &msg);
    // void sendAck(AsyncClient *client, uint8_t from = 0, const char *status = "OK");

    // void sendToCloud(MessageRec &msg);
    /*struct Device
    {
        AsyncClient *client;
        uint8_t id;
        String name;
        String ip;
        Device() : client(nullptr), id(0), name(""), ip("") {}
    };
    */

    // Device devices[MAX_DEVICES];

    void printPasso()
    {
        static int p = 0;
        Serial.print(++p);
        Serial.print(".");
    }
    void setup()
    {
        Serial.begin(115200);
        Serial.println();
        wm.autoConnect();
        Serial.print("Gateway WiFi, iniciando...  ");
        printPasso();
        radio.begin(TERMINAL_ID, 0);
        radio.setTerminalName(TERMINAL_NAME);
        // Configura roteador
        //  router.registerHandler(MSG_TYPE_CMD, handleCommand);
        // router.registerHandler(MSG_TYPE_EVT, handleEvent);
        //    router.registerHandler(MSG_TYPE_ACT, handleActuator);
        printPasso();
        // Configura servidor
        //  tcpServer.onClient([](void *arg, AsyncClient *client)
        //                     { handleNewClient(client); }, nullptr);
        printPasso();
        // tcpServer.begin();
        printPasso();
        Serial.println("Pronto");
    }

    void loop()
    {
        static long updated = 0;

        if (millis() - updated > 10000)
        {
            radio.send(0xFF, EVT_STATUS, "off", TERMINAL_ID == 0 ? 0xFE : TERMINAL_ID);
            updated = millis();
        }
        static long updated0 = 0;
        if (millis() - updated0 > 15000)
        {
            radio.send(0xFF, EVT_PING, TERMINAL_NAME, TERMINAL_ID);
            updated0 = millis();
        }

        radio.loop();
        processMessages();
        delay(10);
    }

    void processMessages()
    {
        MessageRec rec;
        if (radio.processIncoming(rec))
        {
            rec.print();
            if (rec.isEvent(EVT_PING))
            {
                radio.send(rec.from, EVT_PONG, TERMINAL_NAME, TERMINAL_ID);
                return;
            }
        }
    }

    /*


    void handleDisconnect(AsyncClient *client)
    {
        if (!client)
            return;

        Serial.printf("Cliente desconectado: %s\n", client->remoteIP().toString().c_str());

        // Remove da lista de dispositivos
        removeDevice(client);

        // Libera a memória
        client->free();
        delete client;
    }
    void handleNewClient(AsyncClient *client)
    {
        if (!client)
            return;
        for (auto &dev : devices)
        {
            if (!dev.client)
            {
                dev.client = client;
                client->onData([](void *arg, AsyncClient *c, void *data, size_t len)
                               { receiveMessage(c, data, len); }, nullptr);
                // ... outros callbacks
                client->onDisconnect([](void *arg, AsyncClient *c)
                                     { handleDisconnect(c); }, nullptr);
                Serial.println(client->remoteIP());
                // Logger::info("Client Connection: %s ", client->remoteIP().toString());
                break;
            }
        }
    }

    void receiveMessage(AsyncClient *client, void *data, size_t len)
    {
        // uint8_t *msg = (uint8_t *)data;

        Serial.printf("Recebendo(%d): %s \n", len - 5, data + 5);
        // Logger::hex(LogLevel::VERBOSE, (char *)data, len);
        MessageRec rec;
        if (rec.decode((char *)data, len))
        {
            if (!rec.verifyCRC())
            {
                Serial.print("CRC: ");
                Serial.println(rec.calculateCRC(), HEX);
            }
            Serial.printf("[%d-%d:%02X]%d:%d {%s%s}", rec.from, rec.to, rec.id, rec.hop, rec.len, rec.event, rec.value);
            rxQueue.pushItem(rec);
        }
    }

    void handleEvent(MessageRec &msg)
    {
        sendToCloud(msg);
    }

    void registerDevice(AsyncClient *client, MessageRec &msg)
    {
        if (!client || !client->connected())
        {
            Serial.println("Erro: Cliente inválido no registro");
            return;
        }

        for (auto &dev : devices)
        {
            if (dev.client->remoteIP() == client->remoteIP())
            {
                dev.id = msg.from;
                dev.name = msg.value;
                sendAck(dev.client);
                break;
            }
        }

        for (auto &dev : devices)
        {
            if (!dev.client)
            { // Slot vazio
                dev.client = client;
                dev.id = msg.from;
                dev.name = msg.value;

                Serial.printf("Novo dispositivo registrado: %s (ID: %d) IP: %s\n",
                              dev.name.c_str(), dev.id, client->remoteIP().toString().c_str());
                sendAck(client);
                return;
            }
        }

        sendAck(client, 0, "ERROR:MAX_DEVICES");
    }
    */
    /**
     * Envia um ACK genérico para confirmar recebimento
     * @param client Ponteiro para o cliente AsyncClient
     */

    /*
    void sendAck(AsyncClient *client, uint8_t from, const char *status)
    {
       if (!client || !client->connected())
       {
           Serial.println("[ERRO] Cliente inválido para ACK");
           return;
       }

       // Obtém o ID do dispositivo a partir da conexão
       uint8_t deviceId = 0xFF; // Valor padrão para desconhecido
       for (const auto &dev : devices)
       {
           if (dev.client == client)
           {
               deviceId = dev.id;
               break;
           }
       }

       MessageRec ack;
       ack.to = deviceId;
       ack.from = from; // Gateway
       ack.setEvent(RSP_ACK);
       ack.setValue(status);
       ack.updateCRC();

       char buffer[64];
       size_t len = ack.encode(buffer, sizeof(buffer));

       if (client->space() > len)
       {
           client->write(buffer, len);
           client->write("\n", 1); // Delimitador
           Serial.printf("[ACK] Enviado para dispositivo %d\n", deviceId);
       }
       else
       {
           Serial.println("[ERRO] Buffer cheio - ACK não enviado");
           // Adiciona à fila de retransmissão
           txQueue.pushItem(ack);
       }
    }

    void sendToCloud(MessageRec &msg)
    {
       // Implementação do envio HTTP
    }

    */
};

App app;
void setup()
{
    app.setup();
}
void loop()
{
    app.loop();
}