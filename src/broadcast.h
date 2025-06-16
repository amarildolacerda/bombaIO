#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <ESPAsyncTCP.h>
#include <WiFiUdp.h>

#define BROADCAST_PORT 54321 // Porta UDP do broadcast
#define CLIENT_PORT 12346    // Porta do cliente para resposta

typedef void (*BroadcastCallback)(String, int); // Definição do callback

class BroadcastHandler
{
public:
    BroadcastHandler() : callback(nullptr) {}

    void setup()
    {
        Serial.begin(115200);
        WiFi.begin("SSID", "PASSWORD"); // Conecte-se à rede WiFi

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println("\n✅ WiFi conectado!");

        udp.begin(BROADCAST_PORT); // Inicia escuta do broadcast
    }

    void loop()
    {
        int packetSize = udp.parsePacket();
        if (packetSize)
        {
            char buffer[50];
            udp.read(buffer, sizeof(buffer));
            buffer[packetSize] = '\0'; // Garante que seja uma string válida

            Serial.print("📡 Broadcast recebido: ");
            Serial.println(buffer);

            // Extrai IP e porta do servidor
            String serverIP = String(buffer).substring(0, String(buffer).indexOf(":"));
            int serverPort = String(buffer).substring(String(buffer).indexOf(":") + 1).toInt();

            Serial.print("🌐 Servidor encontrado: ");
            Serial.print(serverIP);
            Serial.print(":");
            Serial.println(serverPort);

            // Executa o callback, se definido
            if (callback)
            {
                callback(serverIP, serverPort);
            }

            // Envia resposta ao servidor
            udp.beginPacket(serverIP.c_str(), serverPort);
            udp.print("{device|ESP}");
            udp.endPacket();
            Serial.println("✅ Resposta enviada ao servidor!");
        }
    }

    void setCallback(BroadcastCallback cb)
    {
        callback = cb;
    }

private:
    WiFiUDP udp;
    BroadcastCallback callback;
};
