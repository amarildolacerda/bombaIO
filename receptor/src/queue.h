#ifndef QUEUE_H
#define QUEUE_H

#include "Arduino.h"

#define MAX_EVENT_LEN 16
#define MAX_VALUE_LEN 32
#define MAX_ITEMS 5

struct MessageRec
{
    uint8_t id;
    uint8_t to;
    uint8_t from;
    uint8_t hope;
    char event[MAX_EVENT_LEN];
    char value[MAX_VALUE_LEN];
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
        noInterrupts();
        bool result = false;

        if (count < MAX_ITEMS)
        {
            items[tail] = item;
            tail = (tail + 1) % MAX_ITEMS;
            count++;
            result = true;
#ifdef DEBUG_ON
            Serial.print(" push: ");
            Serial.println(size());
#endif
        }

        interrupts();
        return result;
    }

    bool popItem(MessageRec &item)
    {
        noInterrupts();
        bool result = false;

        if (count > 0)
        {
            item = items[head];
            head = (head + 1) % MAX_ITEMS;
            count--;
            result = true;
        }

        interrupts();
        return result;
    }

    void push(const uint8_t to, const char *event, const char *value,
              const uint8_t from, const uint8_t hope, const uint8_t id = 0)
    {
        MessageRec msg;
        memset(&msg, 0, sizeof(MessageRec));
        msg.to = to;
        msg.from = from;
        msg.hope = hope;
        msg.id = id;
        strncpy(msg.event, event, MAX_EVENT_LEN - 1);
        strncpy(msg.value, value, MAX_VALUE_LEN - 1);
        pushItem(msg);
    }

    bool pop(MessageRec &msg)
    {
        return popItem(msg);
    }

    bool isEmpty()
    {
        noInterrupts();
        bool result = (count == 0);
        interrupts();
        return result;
    }

    bool isFull()
    {
        noInterrupts();
        bool result = (count == MAX_ITEMS);
        interrupts();
        return result;
    }

    int size()
    {
        noInterrupts();
        int result = count;
        interrupts();
        return result;
    }
};

#endif