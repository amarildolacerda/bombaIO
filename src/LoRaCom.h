#ifndef LORACOM_H
#define LORACOM_H

#include "config.h"
#include "SystemState.h"
#include "app_messages.h"
#ifdef WS
#include "ExtraQueue.h"
#endif

#ifdef RF95
#include "LoRaRF95.h"
#elif defined(LORA32) || defined(TTGO) || defined(HELTEC)
#include "LoRa32.h"
#elif NRF24
#include "RadioNRF24.h"
#elif RADIO_WIFI
#include "RadioWiFi.h"
#else
#include "LoRaDummy.h"
#endif

#ifdef BROADCAST
#include "broadcast.h"
#endif

extern void broadcastCallbackFn(const NetworkInfo *info);

class LoRaCom
{
private:
#ifdef BROADCAST
    BroadcastHandler *broadcast = nullptr;
#endif
public:
    RadioInterface *radio = nullptr;
    LoRaCom()
    {
#ifdef RF95
        radio = new LoRaRF95();
#elif defined(LORA32) || defined(TTGO) || defined(HELTEC)
        radio = new LoRa32();
#elif NRF24
        radio = new RadioNRF24();
#elif RADIO_WIFI
        radio = new RadioWiFi(12345, TERMINAL_ID == 0);
#else
        radio = new LoRaDummy();
#endif
    }

    String getIdent()
    {
        return radio->getIdent();
    }
    void send(const uint8_t tid, const String &event, const String &value, const uint8_t from = TERMINAL_ID, uint8_t seq = 0)
    {

        radio->send(tid, event.c_str(), value.c_str(), (TERMINAL_ID == tid) ? 0xFE : from, seq);
    }
    void receive(const uint8_t tid, const String &event, const String &value)
    {
        MessageRec rec;
        rec.clear();
        rec.to = tid;
        rec.from = TERMINAL_ID;
        rec.setEvent(event.c_str());
        rec.setValue(value.c_str());
        // snprintf(rec.event, sizeof(rec.event), "%s", event.c_str());
        // snprintf(rec.value, sizeof(rec.value), "%s", value.c_str());
        rec.hop = ALIVE_PACKET;
        radio->receive(rec);
    }
    void loop()
    {
#ifdef WS
        MessageRec rec;
        if (txExtraQueue.popItem(rec))
            LoRaCom::send(rec.to, rec.event, rec.value);

#endif
        radio->loop();
#ifdef BROADCAST
        broadcast->loop();
#ifdef GATEWAY
        static long broadcastUpdate = 0;
        if (broadcastUpdate == 0 || millis() - broadcastUpdate > 30000)
        {
            broadcast->sendBroadcastRequest();
            broadcastUpdate = millis();
        }
#endif
#endif
    }
    void sendPresentation(const uint8_t tid)
    {
        send(tid, EVT_PRESENTATION, systemState.terminalName);
    }
    bool begin(const uint8_t tid, const long band, const bool promiscuos = true)
    {
#ifdef BROADCAST
        broadcast = new BroadcastHandler(tid == 0);
        broadcast->setup();
        broadcast->setCallback(broadcastCallbackFn);
#endif
        return radio->begin(tid, band, promiscuos);
    }
    int packetRssi()
    {
        return radio->packetRssi();
    }

    int packetSnr()
    {
        return radio->packetSnr();
    }
    bool processIncoming(MessageRec &rec)
    {
        return radio->processIncoming(rec);
    }
    bool isConnected()
    {
        return radio->isConnected();
    }
};

extern LoRaCom loraCom;
#endif