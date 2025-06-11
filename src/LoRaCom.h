#ifndef LORACOM_H
#define LORACOM_H

#include "config.h"

#if defined(TTGO) || defined(HELTEC)
#include "LoRa32.h"
static LoRa32 lora;
#elif RF95
#include "LoRaRF95.h"
static LoraRF lora;
#endif

#ifdef WS
#include "html_tserver.h"
#endif

class LoRaCom
{
public:
    static void send(const uint8_t tid, const String event, const String value)
    {
        lora.send(tid, event.c_str(), value.c_str(), Config::TERMINAL_ID);
    }
    static void receive(const uint8_t tid, const String event, const String value)
    {
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = tid;
        rec.from = Config::TERMINAL_ID;
        snprintf(rec.event, sizeof(rec.event), "%s", event.c_str());
        snprintf(rec.value, sizeof(rec.value), "%s", value.c_str());
        rec.hope = ALIVE_PACKET;
        lora.receive(rec);
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
    }
    static void sendPresentation(const uint8_t tid)
    {
        LoRaCom::send(tid, EVT_PRESENTATION, systemState.terminalName);
    }
};

#endif