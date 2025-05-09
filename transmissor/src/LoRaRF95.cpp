#include "LoRaRF95.h"
#include "logger.h"

// #include <SPI.h>
// #include <SoftwareSerial.h>
#include <RH_RF95.h>
#include "config.h"

// SoftwareSerial NR95Serial(Config::LORA_RX_PIN, Config::LORA_TX_PIN);

// #include <RH_RF95.h>
// RH_RF95<SoftwareSerial> rf95(NR95Serial);
RH_RF95 rf95(Serial);

#ifdef RF95
// Global instance of LoRaRF95
LoRaRF95 LoRa;
#endif

bool LoRaRF95::begin(float frequency, bool promiscuous)

{
    Serial.swap();

    if (!rf95.init())
    {
        Logger::log(LogLevel::INFO, "LoRa initialization failed!");
        return false;
    }
    // LoRaSerial.setTimeout(0);
    rf95.setFrequency(frequency);
    rf95.setPromiscuous(promiscuous);
    rf95.setHeaderTo(0xFF);
    rf95.setTxPower(14);
    rf95.setHeaderFlags(0, RH_FLAGS_NONE);

    Logger::log(LogLevel::INFO, "LoRa Server Ready");
    return true;
}
void LoRaRF95::setSyncWord(const uint8_t tid)
{
    _tid = tid;
    rf95.setHeaderFrom(tid);
}

void LoRaRF95::sendMessage(uint8_t tid, const char *message)
{

    rf95.setHeaderTo(tid);
    rf95.setHeaderId(genHeaderId());
    rf95.send((uint8_t *)message, strlen(message));
    if (!rf95.waitPacketSent())
    {
        Logger::log(LogLevel::ERROR, "Failed to send message");
    }
    else
    {
        Logger::log(LogLevel::DEBUG, message);
    }
}

bool LoRaRF95::receiveMessage(char *buffer, uint8_t &len)
{
    if (rf95.available())
    {
        if (rf95.recv((uint8_t *)buffer, &len))
        {
            buffer[len] = '\0';
            return true;
        }
    }
    return false;
}
static bool printedOk = false;
int LoRaRF95::endPacket()
{
    // return int(printedOk && rf95.waitPacketSent());
    return printedOk;
}
bool LoRaRF95::beginPacket()
{

    return rf95.available();
}
void LoRaRF95::print(const char *message)
{

    rf95.setHeaderTo(_tid);
    rf95.setHeaderId(genHeaderId());
    printedOk = rf95.send((uint8_t *)message, strlen(message));
}

int16_t LoRaRF95::packetRssi()
{
    return rf95.lastRssi();
}
bool LoRaRF95::parsePacket()
{
    return rf95.available();
}
bool LoRaRF95::available()
{
    return rf95.available();
}

uint8_t LoRaRF95::read()
{
    if (!rf95.available())
        return 0;
    uint8_t buf[1] = {0};
    uint8_t len = sizeof(buf);
    bool rt =
        !rf95.recv(buf, &len);
    if (rt && len > 0)
    {
        return buf[0];
    }
    return 0;
}

uint8_t LoRaRF95::genHeaderId()
{
    if (nHeaderId >= 255)
        nHeaderId = 0;
    return nHeaderId++;
}
