
#pragma
#include "LoRaRF95.h"
#include "queue_message.h"
#include "logger.h"
#include "app_messages.h"

namespace AppProcess
{
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? EVT_ACK : EVT_NAK, "", Config::TERMINAL_ID);
    }

    void setStatusChanged()
    {
        lora.send(0, EVT_STATUS, digitalRead(Config::RELAY_PIN) ? GPIO_ON : GPIO_OFF, Config::TERMINAL_ID);
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

        if (strstr(rec.event, EVT_ACK))
            return true;
        if (strstr(rec.event, EVT_NAK))
            return false;

        if (strstr(rec.event, EVT_GPIO))
        {
            if (strstr(rec.value, GPIO_ON))
            {
                digitalWrite(Config::RELAY_PIN, HIGH);
                handled = true;
            }
            else if (strstr(rec.value, GPIO_OFF))
            {
                digitalWrite(Config::RELAY_PIN, LOW);
                handled = true;
            }
            else if (strstr(rec.value, GPIO_TOGGLE))
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                handled = true;
            }
            setStatusChanged();
        }

        return true;

        if (strstr(rec.event, EVT_PRESENTATION))
        {

            lora.send(0, EVT_PRESENTATION, Config::TERMINAL_NAME, Config::TERMINAL_ID);
            return true;
        }

        if (strstr(rec.event, EVT_STATUS))
        {
            setStatusChanged();
            return true;
        }
        else if (strstr(rec.event, EVT_RESET))
        {
            return true;
        }

        ack(rec.from, handled);
        return handled;
    }

}