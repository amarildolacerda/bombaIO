#include "LoRaSerial.h"
#include <SoftwareSerial.h>
#include "logger.h"

// Define the RX and TX pins for the software serial

// Create a SoftwareSerial instance
SoftwareSerial loraSerial(Config::LORA_RX_PIN, Config::LORA_TX_PIN);

#ifdef LORASERIAL
LoRaSerial LoRa;
#endif

void sendATCommand(String cmd)
{
    loraSerial.println(cmd);
    // debugSerial("ENVIADO", (uint8_t *)cmd.c_str(), cmd.length());
    Logger::info("ENVIADO");

    unsigned long start = millis();
    while (!loraSerial.available() && (millis() - start < 5000))
    {
        delay(10);
    }
    while (loraSerial.available())
    {
        String response = loraSerial.readString();
        Logger::debug(response.c_str());
        // debugSerial("RESPOSTA", (uint8_t *)response.c_str(), response.length());
    }
}

// No setup():
// sendATCommand("AT+ADDRESS?");

bool LoRaSerial::begin(float frequency, bool promiscuous)
{
    // Serial.swap();
    loraSerial.begin(115200);
    sendATCommand("AT+FREQ=" + String(frequency));
    sendATCommand("AT");
    return true;
}

size_t sentBytes = 0;

void LoRaSerial::sendMessage(uint8_t tid, const char *message)
{
    digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
    sentBytes = loraSerial.write(message);
    // Serial1.print(message);
    unsigned long start = millis();
    while (!loraSerial.available() && (millis() - start < 500))
    {
        delay(10);
    }
    digitalWrite(LED_BUILTIN, LOW);
    // Turn off LED
    Serial.println(loraSerial.readString());
}

bool LoRaSerial::receiveMessage(char *buffer, uint8_t &len)
{
    if (loraSerial.available())
    {
        len = loraSerial.readBytes(buffer, 255);
        buffer[len] = '\0';
        return true;
    }
    return false;
}
int16_t LoRaSerial::packetRssi()
{
    return 0;
}
uint8_t LoRaSerial::genHeaderId()
{
    return nHeaderId++;
}
void LoRaSerial::setSyncWord(const uint8_t tid)
{
    _tid = tid;
}

void LoRaSerial::print(const char *message)
{
    sendMessage(_tidTo, message);
}
bool LoRaSerial::beginPacket()
{
    return true;
}
int LoRaSerial::endPacket()
{
    return sentBytes;
}
bool LoRaSerial::parsePacket()
{
    return true;
}
bool LoRaSerial::available()
{
    return loraSerial.available();
}
byte LoRaSerial::read()
{
    return loraSerial.read();
}
