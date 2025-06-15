#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include "queue_message.h"

// Configurações de rede
#define WIFI_SSID "sua_rede_wifi"
#define WIFI_PASSWORD "sua_senha"
#define TCP_PORT 12345
#define DISCOVERY_PORT 12346
#define CLOUD_SERVER "http://seuservidor.com/api/data"

// Estrutura para armazenar informações dos dispositivos
struct DeviceInfo
{
    WiFiClient client;
    String name;
    unsigned long lastSeen;
    uint8_t terminalId;
};

class Gateway
{
private:
    WiFiServer tcpServer{TCP_PORT};
    WiFiUDP udp;
    HTTPClient http;
    WiFiClient wifiClient;

    FifoList rxQueue{10};
    FifoList txQueue{10};

    std::vector<DeviceInfo> devices;
    unsigned long lastDiscovery = 0;
    const unsigned long discoveryInterval = 30000; // 30 segundos
    const unsigned long cleanupInterval = 60000;   // 1 minuto
    unsigned long lastCleanup = 0;

    uint8_t gatewayId = 0;

public:
    void begin()
    {
        Serial.begin(115200);

        // Conecta ao WiFi
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        Serial.print("Conectado! IP: ");
        Serial.println(WiFi.localIP());

        // Inicia servidores
        tcpServer.begin();
        tcpServer.setNoDelay(true);
        udp.begin(DISCOVERY_PORT);

        Serial.println("Gateway iniciado");
    }

    void loop()
    {
        handleNewClients();
        processTCPMessages();
        processUDPDiscovery();
        processOutgoingMessages();
        cleanupDevices();
        sendToCloud();
    }

private:
    void handleNewClients()
    {
        WiFiClient newClient = tcpServer.accept();
        if (newClient)
        {
            DeviceInfo device;
            device.client = newClient;
            device.lastSeen = millis();
            devices.push_back(device);

            Serial.printf("Novo cliente conectado: %s\n",
                          newClient.remoteIP().toString().c_str());
        }
    }

    void processTCPMessages()
    {
        for (auto &device : devices)
        {
            if (!device.client.connected())
                continue;

            while (device.client.available())
            {
                String data = device.client.readStringUntil('\n');
                MessageRec msg;

                if (msg.decode(data.c_str(), data.length()) && msg.verifyCRC())
                {
                    device.lastSeen = millis();

                    if (msg.isEvent("IDENTIFY"))
                    {
                        handleIdentification(device, msg);
                    }
                    else
                    {
                        rxQueue.pushItem(msg);
                        Serial.printf("Mensagem de %d: %s|%s\n",
                                      msg.from, msg.event, msg.value);
                    }
                }
            }
        }
    }

    void handleIdentification(DeviceInfo &device, MessageRec &msg)
    {
        device.terminalId = msg.from;
        device.name = msg.value;

        // Responde com confirmação
        MessageRec ack;
        ack.to = device.terminalId;
        ack.from = gatewayId;
        ack.setEvent("IDENTIFY_ACK");
        ack.setValue("OK");
        ack.updateCRC();

        sendToDevice(device, ack);

        Serial.printf("Dispositivo registrado: %s (ID: %d)\n",
                      device.name.c_str(), device.terminalId);
    }

    void processUDPDiscovery()
    {
        // Envia broadcast periódico
        if (millis() - lastDiscovery > discoveryInterval)
        {
            sendDiscoveryBroadcast();
            lastDiscovery = millis();
        }

        // Processa respostas
        int packetSize = udp.parsePacket();
        if (packetSize)
        {
            char buffer[128];
            int len = udp.read(buffer, sizeof(buffer) - 1);
            buffer[len] = '\0';

            MessageRec msg;
            if (msg.decode(buffer, len) && msg.isEvent("DISCOVER_RESP"))
            {
                IPAddress remoteIP = udp.remoteIP();

                // Verifica se já está na lista
                bool exists = false;
                for (auto &d : devices)
                {
                    if (d.client.remoteIP() == remoteIP)
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                {
                    Serial.printf("Novo dispositivo descoberto: %s (ID: %d)\n",
                                  remoteIP.toString().c_str(), msg.from);

                    // Conecta via TCP
                    WiFiClient newClient;
                    if (newClient.connect(remoteIP, TCP_PORT))
                    {
                        DeviceInfo newDevice;
                        newDevice.client = newClient;
                        newDevice.terminalId = msg.from;
                        newDevice.name = msg.value;
                        newDevice.lastSeen = millis();
                        devices.push_back(newDevice);
                    }
                }
            }
        }
    }

    void sendDiscoveryBroadcast()
    {
        IPAddress broadcastIP = WiFi.localIP();
        broadcastIP[3] = 255; // 192.168.1.255

        MessageRec msg;
        msg.to = 0xFF; // Broadcast
        msg.from = gatewayId;
        msg.setEvent("DISCOVER");
        msg.setValue(WiFi.localIP().toString().c_str());
        msg.updateCRC();

        char buffer[128];
        size_t len = msg.encode(buffer, sizeof(buffer));

        udp.beginPacket(broadcastIP, DISCOVERY_PORT);
        udp.write(buffer, len);
        udp.endPacket();

        Serial.println("Enviado broadcast de descoberta");
    }

    void processOutgoingMessages()
    {
        MessageRec msg;
        while (txQueue.popItem(msg))
        {
            if (msg.to == 0xFF)
            { // Broadcast
                for (auto &device : devices)
                {
                    sendToDevice(device, msg);
                }
            }
            else
            {
                // Envia para dispositivo específico
                for (auto &device : devices)
                {
                    if (device.terminalId == msg.to)
                    {
                        sendToDevice(device, msg);
                        break;
                    }
                }
            }
        }
    }

    void sendToDevice(DeviceInfo &device, MessageRec &msg)
    {
        if (!device.client.connected())
            return;

        char buffer[128];
        size_t len = msg.encode(buffer, sizeof(buffer));
        device.client.write(buffer, len);
        device.client.write('\n');
    }

    void cleanupDevices()
    {
        if (millis() - lastCleanup > cleanupInterval)
        {
            for (auto it = devices.begin(); it != devices.end();)
            {
                if (!it->client.connected() ||
                    (millis() - it->lastSeen > cleanupInterval * 2))
                {
                    Serial.printf("Dispositivo desconectado: %s (ID: %d)\n",
                                  it->name.c_str(), it->terminalId);
                    it = devices.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            lastCleanup = millis();
        }
    }

    void sendToCloud()
    {
        MessageRec msg;
        while (rxQueue.popItem(msg))
        {
            if (WiFi.status() != WL_CONNECTED)
                continue;

            http.begin(wifiClient, CLOUD_SERVER);
            http.addHeader("Content-Type", "application/json");

            String payload = String() +
                             "{\"device_id\":" + msg.from +
                             ",\"event\":\"" + msg.event +
                             "\",\"value\":\"" + msg.value + "\"}";

            int httpCode = http.POST(payload);
            if (httpCode > 0)
            {
                Serial.printf("Dados enviados para nuvem. Código: %d\n", httpCode);
            }
            else
            {
                Serial.printf("Falha ao enviar para nuvem: %s\n",
                              http.errorToString(httpCode).c_str());
            }

            http.end();
        }
    }
};
