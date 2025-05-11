#include <Arduino.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "RH_RF95_UART.h"
#include "RH_RF95.h"

// Configuração do Hardware
#define LORA_RX D1          //
#define LORA_TX D2          //
#define TERMINAL_ID 0x01    // ID deste dispositivo
#define DESTINATION_ID 0x00 // ID do terminal destino

EspSoftwareSerial::UART loraUART(LORA_RX, LORA_TX);
RH_RF95_UART<EspSoftwareSerial::UART> rf95(loraUART);

// Contador de pacotes
uint16_t packetCounter = 0;

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // Espera Serial no debug

    // Inicializa LoRa
    loraUART.begin(9600);

    if (!rf95.init())
    {
        Serial.println("Falha na inicialização do LoRa!");
        while (1)
            ;
    }

    rf95.setFrequency(915.0);
    rf95.setTxPower(14);
    rf95.setThisAddress(TERMINAL_ID);

    Serial.println("LoRa JSON Sender/RECEIVER pronto!");
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

    Serial.print("Enviando: ");
    Serial.println(jsonStr);

    rf95.send((uint8_t *)jsonStr.c_str(), jsonStr.length());
    rf95.waitPacketSent();
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

            Serial.print("Recebido [RSSI:");
            Serial.print(rf95.lastRssi());
            Serial.print("dBm]: ");
            Serial.println((char *)buf);

            // Processa JSON recebido
            StaticJsonDocument<200> doc;
            DeserializationError error = deserializeJson(doc, buf);

            if (!error)
            {
                Serial.print("Mensagem de: ");
                Serial.println(doc["device"].as<String>());
            }
            else
            {
                Serial.print("Erro no JSON: ");
                Serial.println(error.c_str());
            }
        }
    }
}

void loop()
{
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