#ifndef LORACOM_H
#define LORACOM_H

#include <Arduino.h>
#include "LoRaInterface.h"

class LoRaCom
{
private:
    static LoRaInterface *loraInstance;

public:
    static void setInstance(LoRaInterface *instance)
    {
        loraInstance = instance;
    }
    static bool initialize();
    static bool sendCommand(const String event, const String value, uint8_t tid = 0xFF);
    static void handle();
    static bool waitAck();
    static void sendPresentation(const uint8_t tid = 0xFF, const uint8_t n = 3);
    static void ack(bool ak = true, uint8_t tid = 0xFF);
    static void formatMessage(char *message, uint8_t tid, const char *event, const char *value);
    static void sleep(unsigned int duration);
    static uint8_t genHeaderId();
    static void sendHeaderTo(uint8_t tid);
    static int packedRssi();
    static void processIncoming();
};

#endif // LORACOM_H