#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "event_types.h"
#include "queue_message.h"
#include <functional>

class MessageRouter
{
public:
    using HandlerFn = std::function<void(MessageRec &)>;

    void registerHandler(uint8_t msgType, HandlerFn handler)
    {
        handlers[msgType] = handler;
    }

    void routeMessage(MessageRec &msg)
    {
        uint8_t msgType = getMessageType(msg.event);
        if (handlers[msgType])
        {
            handlers[msgType](msg);
        }
        else
        {
            defaultHandler(msg);
        }
    }

    static uint8_t getMessageType(const char *event)
    {
        for (const auto &meta : EVENT_TABLE)
        {
            if (strcmp(meta.name, event) == 0)
            {
                return meta.type;
            }
        }
        return MSG_TYPE_CMD; // Default
    }

private:
    HandlerFn handlers[6]; // Para cada tipo de mensagem
    HandlerFn defaultHandler = [](MessageRec &msg)
    {
        Serial.printf("No handler for: %s\n", msg.event);
    };
};

#endif