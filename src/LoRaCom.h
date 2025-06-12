#ifndef LORACOM_H
#define LORACOM_H

#include "config.h"
#include "SystemState.h"
#include "app_messages.h"

#if defined(TTGO) || defined(HELTEC)
#include "LoRa32.h"
LoRa32 radio;
#elif RF95
#include "LoRaRF95.h"
LoRaRF95 radio;
#elif NRF24
#include "RadioNRF24.h"
RadioNRF24 radio;
#elif DUMMY
#include "LoRaDummy.h"
LoRaDummy radio;
#endif

#ifdef WS
#include "html_tserver.h"
#endif

class LoRaCom
{
public:
    static void send(const uint8_t tid, const String &event, const String &value)
    {
        radio.send(tid, event.c_str(), value.c_str(), TERMINAL_ID);
    }
    static void receive(const uint8_t tid, const String &event, const String &value)
    {
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = tid;
        rec.from = TERMINAL_ID;
        snprintf(rec.event, sizeof(rec.event), "%s", event.c_str());
        snprintf(rec.value, sizeof(rec.value), "%s", value.c_str());
        rec.hope = ALIVE_PACKET;
        radio.receive(rec);
    }
    static void loop()
    {
#ifdef WS
        if (htmlServer.txRec.to != 0)
        {
            LoRaCom::send(htmlServer.txRec.to, htmlServer.txRec.event, htmlServer.txRec.value);
            htmlServer.txRec.to = 0;
        }
#endif
        radio.loop();
    }
    static void sendPresentation(const uint8_t tid)
    {
        LoRaCom::send(tid, EVT_PRESENTATION, systemState.terminalName);
    }
    static bool begin(const uint8_t tid, const long band, const bool promiscuos = true)
    {
        return radio.begin(tid, band, promiscuos);
    }
    static int packetRssi()
    {
        return radio.packetRssi();
    }

    static int packetSnr()
    {
        return radio.packetSnr();
    }
};

#endif