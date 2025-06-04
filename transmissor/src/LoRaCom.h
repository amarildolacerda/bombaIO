#ifndef LORACOM_H
#define LORACOM_H

// #include <Arduino.h>
#include "LoRaInterface.h"
#include "queue_message.h"

class LoRaCom
{
private:
    static LoRaInterface *loraInstance;
    static bool loraConnected;
    uint8_t nHeaderId = 0;

protected:
    static void (*onReceiveCallback)(MessageRec *);

public:
    static void setReceiveCallback(void (*callback)(MessageRec *));
    //{
    //    onReceiveCallback = callback;
    // }
    static void setInstance(LoRaInterface *instance)
    {
        loraInstance = instance;
    }
    static bool initialize();
    static bool sendCommand(const String event, const String value, uint8_t tid = 0xFF);
    static void handle();
    static bool waitAck();
    static void sendPresentation(const uint8_t tid = 0xFF, const uint8_t n = 3);
    static void sendTime();

    static void ack(bool ak = true, uint8_t tid = 0xFF);
    static void formatMessage(char *message, uint8_t tid, const char *event, const char *value);
    static void sleep(unsigned int duration);
    static uint8_t genHeaderId();
    static void sendHeaderTo(uint8_t tid);
    static int packetRssi();
};

#endif // LORACOM_H