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

class LoRaCom
{
public:
    static void send(String event, String value, uint8_t tid)
    {
        lora.send(tid, event.c_str(), value.c_str(), Config::TERMINAL_ID);
    }
};

#endif