#ifndef QUEUEMESSAGE_H
#define QUEUEMESSAGE_H

#include "Arduino.h"
#if defined(__AVR__)
// #include <util/atomic.h>
// #define CRITICAL_SECTION ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#elif defined(ESP32)
#define LoRaWAN_DEBUG_LEVEL 1 // Ou outro valor entre 0-4
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define CRITICAL_SECTION ;

// Versão 1: Usando portENTER_CRITICAL (recomendada para ESP32)
// #define CRITICAL_SECTION                                            \
//    for (portMUX_TYPE __tempMutex__ = portMUX_INITIALIZER_UNLOCKED; \
//         (portENTER_CRITICAL(&__tempMutex__), true);                \
//         portEXIT_CRITICAL(&__tempMutex__))

#else
#error "Plataforma não suportada"
#endif

#define MAX_EVENT_LEN 8
#define MAX_VALUE_LEN 24
#ifdef ESP32
#define MAX_ITEMS 5
#else
#define MAX_ITEMS 2
#endif
struct MessageRec
{
    uint8_t id;
    uint8_t to;
    uint8_t from;
    uint8_t hope;
    char event[MAX_EVENT_LEN];
    char value[MAX_VALUE_LEN];
    int dv() const
    {
        return id + to + from + sizeof(event);
    }
#ifdef DEBUG_ON
    void print()
    {
        char msg[100] = {0};
        snprintf(msg, sizeof(msg), "%d[%d-%d:%d](%d) {%s|%s}", from, from, to, id, hope, event, value);
        Serial.println(msg);
    }
#endif
};

class FifoList
{
private:
    MessageRec items[MAX_ITEMS];
    volatile int head = 0;
    volatile int tail = 0;
    volatile int count = 0;

public:
    bool pushItem(const MessageRec &item)
    {
        bool result = false;

        if (checkDup)
        {
            if (contains(item))
                return false;
        }
        // CRITICAL_SECTION
        // {
#ifdef DEBUG_ON
        Serial.print("Push item: ");
        Serial.println(count + 1);
#endif
        if (count < MAX_ITEMS)
        {
            items[tail] = item;
            tail = (tail + 1) % MAX_ITEMS;
            count++;
            result = true;
#ifdef DEBUG_ON
            //       Serial.print(" push: ");
            //       Serial.println(size());
#endif
        }
        //}
        return result;
    }

    bool contains(MessageRec item) const
    {
        bool result = false;
        // CRITICAL_SECTION
        // {
        for (int i = 0; i < count; i++)
        {
            int index = (head + i) % MAX_ITEMS;
            if (items[index].dv() == item.dv())
            {
                result = true;
                break;
            }
        }
        //}
        return result;
    }
    bool checkDup = false;
    bool popItem(MessageRec &item)
    {

        bool result = false;
        // CRITICAL_SECTION
        // {

        if (count > 0)
        {
            item = items[head];
            head = (head + 1) % MAX_ITEMS;
            count--;
            result = true;
        }
        //}
        return result;
    }

    bool push(const uint8_t to, const char *event, const char *value,
              const uint8_t from, const uint8_t hope, const uint8_t id = 0)
    {
        MessageRec msg;
        memset(&msg, 0, sizeof(MessageRec));
        msg.to = to;
        msg.from = from;
        msg.hope = hope;
        msg.id = id;
        strncpy(msg.event, event, MAX_EVENT_LEN);
        strncpy(msg.value, value, MAX_VALUE_LEN);
        return pushItem(msg);
    }

    bool isEmpty()
    {
        bool result = false;
        // CRITICAL_SECTION
        // {
        result = (count == 0);
        // }
        return result;
    }

    bool isFull()
    {
        bool result = false;
        // CRITICAL_SECTION
        // {
        result = (count == MAX_ITEMS);
        // }
        return result;
    }

    int size()
    {
        int result = 0;
        // CRITICAL_SECTION
        //{
        result = count;
        //}
        return result;
    }
};

#endif