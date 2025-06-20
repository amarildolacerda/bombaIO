#ifndef LORA32_H
#define LORA32_H

#include <SPI.h> // include libraries
#include <LoRa.h>
#include <RadioInterface.h>
#include <config.h>
#include <app_messages.h>
#include "logger.h"
#include "stats.h"
const int csPin = 18;    // LoRa radio chip select
const int resetPin = 14; // LoRa radio reset
const int irqPin = 26;   // change for your board; must be a hardware interrupt pin

class LoRa32 : public RadioInterface
{
public:
    bool sendMessage(MessageRec &rec) override
    {
        stats.txCount++;
        String outgoing = "{" + String(rec.event) + "|" + String(rec.value) + "}";
        LoRa.beginPacket();                // start packet
        LoRa.write(rec.to);                // add destination address
        LoRa.write(rec.from);              // add sender address
        LoRa.write(rec.id);                // add message ID
        LoRa.write(outgoing.length() + 1); // add payload length
        LoRa.write(rec.hope);
        LoRa.print(outgoing); // add payload
        if (LoRa.endPacket() > 0)
        {
            stats.txSuccess++;
        }; // finish packet and send it
        String fmt = String(rec.hope < 3 ? "MESH " : "") + "[%d-%d:%d](%d) %s|%s";

        Logger::log(LogLevel::SEND, fmt.c_str(), rec.from, rec.to, rec.id, rec.hope, rec.event, rec.value);
        return true;
    };

    bool receiveMessage() override
    {
        stats.rxCount++;

        int packetSize = LoRa.parsePacket();
        if (packetSize == 0)
            return false; // if there's no packet, return

        // read packet header bytes:
        MessageRec rec;
        memset(&rec, 0, sizeof(rec));

        // read packet header bytes:
        rec.to = LoRa.read();              // recipient address
        rec.from = LoRa.read();            // sender address
        rec.id = LoRa.read();              // incoming msg ID
        byte incomingLength = LoRa.read(); // incoming msg length
        rec.hope = LoRa.read();

        String incoming = "";

        uint8_t len = 0;
        long updated = millis();
        while (len <= packetSize)
        {
            if (!LoRa.available() && (millis() - updated > 50))
                break;
            if (LoRa.available())
            {
                incoming += (char)LoRa.read();
                len++;
                updated = millis();
            }
        }

        if (!parseRcv(rec, incoming))
            return false;

        if (rec.from == terminalId)
        {
            return false; // skip messages from myself
        }

        meshMessage(rec);

        if (rec.to != terminalId && rec.to != 0xFF)
        {
            Serial.println("This message is not for me.");
            return false; // skip rest of function
        }

        rxQueue.pushItem(rec); // add to rx queue
        Logger::log(LogLevel::RECEIVE, "[%d-%d:%d](%d) %s|%s", rec.from, rec.to, rec.id, rec.hope, rec.event, rec.value);
        stats.rxSuccess++;
        return true;
    };
    void configParams()
    {
        // LoRa.setSpreadingFactor(LORA_SF); NAO USAR
        // #ifdef LORA_CRC
        LoRa.enableCrc();
        // #endif
    }
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;
        // override the default CS, reset, and IRQ pins (optional)
        LoRa.setPins(csPin, resetPin, irqPin); // set CS, reset, IRQ pin

        if (!LoRa.begin(band))
        { // initialize ratio at 915 MHz
            Serial.println("LoRa init failed. Check your connections.");
            while (true)
                ; // if failed, do nothing
        }
        else
            connected = true;
        configParams();

        return isConnected();
    };
    int packetRssi() override
    {
        return LoRa.packetRssi(); // return the packet RSSI
    }
    int packetSnr() override
    {
        return LoRa.packetSnr();
    }
};

#endif
