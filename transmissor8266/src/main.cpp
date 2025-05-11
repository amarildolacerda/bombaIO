#include <Arduino.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "RH_RF95.h"
#include <WiFiManager.h>
#include <WebSocketsServer.h>

// Configuração do Hardware
#define LORA_RX D7          //
#define LORA_TX D8          //
#define TERMINAL_ID 100     // ID deste dispositivo
#define DESTINATION_ID 0x00 // ID do terminal destino

// EspSoftwareSerial::UART loraUART(LORA_RX, LORA_TX);
// RH_RF95_UART<EspSoftwareSerial::UART> rf95(loraUART);

RH_RF95 rf95(Serial);

// Configuração do WebSocket
WebSocketsServer webSocket(81);

// Configuração da rede Wi-Fi
const char *ssid = "kcasa";
const char *password = "3938373635";

// Contador de pacotes
uint16_t packetCounter = 0;

void RfPrint(String msg)
{
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = msg.length();
    memcpy(buf, msg.c_str(), len);
    buf[len] = '\0'; // Null-terminate
    rf95.send(buf, len);
    rf95.waitPacketSent();
}

void debug(String msg)
{
}
void debugln(String msg)
{
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    if (type == WStype_TEXT)
    {
        /*  debug("[WebSocket] Mensagem recebida de ");
          debug((String)num);
          debug(": ");
          debugln((char *)payload);
          */
    }
}

void sendWebSocketDebug(const String &message)
{
    webSocket.broadcastTXT(message.c_str());
}

void connectToWiFi()
{
    WiFiManager wifiManager;

    // wifiManager.erase();
    wifiManager.setDebugOutput(true);
    // Inicia o WiFiManager e tenta conectar
    if (!wifiManager.autoConnect("AutoConnectAP"))
    {
        Serial.print("Falha ao conectar ao Wi-Fi e sem configuração salva.");
        ESP.restart();
    }

    Serial.println("Conectado ao Wi-Fi!");
    Serial.print("Endereço IP: ");
    String ip = WiFi.localIP().toString();
    Serial.println(ip);
}

void setup()
{
    Serial.begin(115200);
    connectToWiFi();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.swap(); // Troca os pinos RX e TX
    Serial.begin(9600);
    while (!Serial)
        ; // Espera Serial no debug

    if (!rf95.init())
    {
        String errorMsg = "Falha na inicialização do LoRa!";
        debugln(errorMsg);
        // ESP.reset();
        //  while (1)
        //      ;
    }

    rf95.setFrequency(868.0);
    rf95.setTxPower(14);
    rf95.setThisAddress(TERMINAL_ID);
    rf95.setPromiscuous(true);

    String readyMsg = "LoRa JSON Sender/RECEIVER pronto!";
    debugln(readyMsg);
}

void sendJsonPacket()
{
    // Cria o documento JSON
    StaticJsonDocument<200> doc;
    doc["device"] = "NodeMCU";
    doc["packet_id"] = packetCounter++;
    doc["temp"] = random(20, 30); // Simula temperatura
    doc["hum"] = random(40, 80);  // Simula umidade

    // Serializa JSON para string
    String jsonStr;
    serializeJson(doc, jsonStr);

    // Configura cabeçalho e envia
    rf95.setHeaderTo(DESTINATION_ID);
    rf95.setHeaderFrom(TERMINAL_ID);
    rf95.setHeaderId(packetCounter);

    debug("Enviando: ");
    // debugln(jsonStr);
    RfPrint(jsonStr);
}

void checkIncomingMessages()
{
    if (rf95.available())
    {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (rf95.recv(buf, &len))
        {
            buf[len] = '\0'; // Null-terminate

            String receivedMsg = String((char *)buf);
            debug("Recebido [RSSI:");
            debug((String)rf95.lastRssi());
            debug("dBm]: ");
            debug(receivedMsg);

            // Processa JSON recebido
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, buf);

            if (!error)
            {
                String sender = doc["device"].as<String>();
                debug("Mensagem de: ");
                debugln(sender);
            }
            else
            {
                String errorMsg = "Erro no JSON: " + String(error.c_str());
                debugln(errorMsg);
            }
            RfPrint("ACK"); // Envia ACK de volta
        }
    }
}

void loop()
{
    webSocket.loop();

    static unsigned long lastSend = 0;

    // Envia a cada 1000ms
    if (millis() - lastSend >= 1000)
    {
        sendJsonPacket();
        lastSend = millis();
    }

    // Verifica mensagens recebidas
    checkIncomingMessages();

    // Pequena pausa para yield()
    delay(10);
}