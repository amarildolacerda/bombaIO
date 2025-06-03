
#pragma
#include "LoRaRF95.h"
#include "queue_message.h"
#include "logger.h"

namespace AppProcess
{
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? "ack" : "nak", "", Config::TERMINAL_ID);
    }

    void setStatusChanged()
    {
        lora.send(0, "status", digitalRead(Config::RELAY_PIN) ? "on" : "off", Config::TERMINAL_ID);
    }

    static long lastStatusSend = 0;
    bool handle()
    {
        if (millis() - lastStatusSend > 5000)
        {
            setStatusChanged();
            lastStatusSend = millis();
        }
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));

        bool hasMessage = lora.processIncoming(rec);

        if (!hasMessage)
        {
            return false;
        }
        Logger::info("Handled from: %d: %s|%s", rec.from, rec.event, rec.value);

        bool handled = false;

        if (strstr(rec.event, "ack"))
            return true;
        if (strstr(rec.event, "nak"))
            return false;

        if (strstr(rec.event, "gpio"))
        {
            if (strstr(rec.value, "on"))
            {
                digitalWrite(Config::RELAY_PIN, HIGH);
                handled = true;
            }
            else if (strstr(rec.value, "off"))
            {
                digitalWrite(Config::RELAY_PIN, LOW);
                handled = true;
            }
            else if (strstr(rec.value, "toggle"))
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                handled = true;
            }
            setStatusChanged();
        }

        return true;

        if (strstr(rec.event, "pub"))
        {

            lora.send(0, "pub", Config::TERMINAL_NAME, Config::TERMINAL_ID);
            return true;
        }

        if (strstr(rec.event, "status"))
        {
            setStatusChanged();
            return true;
        }
        else if (strstr(rec.event, "reset"))
        {
            return true;
        }

        ack(rec.from, handled);
        return handled;
    }

}