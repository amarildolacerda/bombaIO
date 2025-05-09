#ifndef LORACOM_H
#define LORACOM_H

#include <Arduino.h>

namespace LoRaCom
{
    bool initialize();
    bool sendCommand(const String event, const String value, uint8_t tid = 0xFF);
    void processIncoming();
    bool waitAck();
    void sendPresentation(uint8_t tid = 0xFF, uint8_t n = 3);
    void ack(bool ak = true, uint8_t tid = 0xFF);
    void formatMessage(char *message, uint8_t tid, const char *event, const char *value);
    void sleep(unsigned int duration);
    uint8_t genHeaderId();
    void sendHeaderTo(uint8_t tid);
}

#endif // LORACOM_H