#ifndef QUEUEMESSAGE_H
#define QUEUEMESSAGE_H

#include "Arduino.h"

#if defined(__AVR__)
#include <util/atomic.h>
#define CRITICAL_SECTION ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#elif defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static portMUX_TYPE queueMutex = portMUX_INITIALIZER_UNLOCKED;
#define CRITICAL_SECTION             \
    portENTER_CRITICAL(&queueMutex); \
    portEXIT_CRITICAL(&queueMutex);
#else
#define CRITICAL_SECTION
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
#ifndef __AVR__
    int dv() const
    {
        return id + to + from + sizeof(event);
    }
#endif

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
    MessageRec *items; // Ponteiro para o array dinâmico
    volatile int head = 0;
    volatile int tail = 0;
    volatile int count = 0;
    volatile int maxItems; // Tamanho máximo da fila

public:
    bool checkDup = false;

    // Construtor que recebe o tamanho da fila
    FifoList(int size = MAX_ITEMS) : maxItems(size)
    {
        items = new MessageRec[maxItems];
    }

    // Destrutor para liberar memória
    ~FifoList()
    {
        delete[] items;
    }

    bool pushItem(const MessageRec &item)
    {
        bool result = false;

        if (checkDup && contains(item))
            return false;

        CRITICAL_SECTION
        {
            if (count < maxItems)
            {
                items[tail] = item;
                tail = (tail + 1) % maxItems;
                count++;
                result = true;
#ifdef DEBUG_ON
                Serial.print("Push item. Count: ");
                Serial.println(count);
#endif
            }
        }
        return result;
    }

    bool contains(const MessageRec &item) const
    {
#ifdef __AVR__
        return false;
#else
        bool result = false;
        CRITICAL_SECTION
        {
            for (int i = 0; i < count; i++)
            {
                int index = (head + i) % maxItems;
                if (items[index].dv() == item.dv())
                {
                    result = true;
                    break;
                }
            }
        }
        return result;
#endif
    }

    bool popItem(MessageRec &item)
    {
        bool result = false;
        CRITICAL_SECTION
        {
            if (count > 0)
            {
                item = items[head];
                head = (head + 1) % maxItems;
                count--;
                result = true;
#ifdef DEBUG_ON
                Serial.print("Pop item. Count: ");
                Serial.println(count);
#endif
            }
        }
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
        bool result;
        CRITICAL_SECTION
        {
            result = (count == 0);
        }
        return result;
    }

    bool isFull()
    {
        bool result;
        CRITICAL_SECTION
        {
            result = (count == maxItems);
        }
        return result;
    }

    int size()
    {
        int result;
        CRITICAL_SECTION
        {
            result = count;
        }
        return result;
    }
};

#endif