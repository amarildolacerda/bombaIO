
#ifndef APPPROCESS_H
#define APPPROCESS_H
#ifdef HELTEC
// #include "LoRa32.h"
#include "LoRaHeltec.h"
#elif TTGO
#include "LoRa32.h"
#else
#include "LoRaRF95.h"
#endif
#include "queue_message.h"
#include "logger.h"
#include "app_messages.h"
#include "stats.h"
#ifdef DISPLAY_ENABLED
#include "display_mgr.h"
#endif

namespace AppProcess
{
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? EVT_ACK : EVT_NAK, "", Config::TERMINAL_ID);
    }

    static long lastStatusSend = 0;
    void setStatusChanged()
    {
        lora.send(0, EVT_STATUS, digitalRead(Config::RELAY_PIN) ? GPIO_ON : GPIO_OFF, Config::TERMINAL_ID);
        lastStatusSend = millis();
        stats.print();
    }

    bool handle()
    {

        if (millis() - lastStatusSend > Config::STATUS_INTERVAL)
        {
            setStatusChanged();
#ifdef DISPLAY_ENABLED
            displayManager.rssi = lora.packetRssi();
            displayManager.snr = lora.packetSnr();
            displayManager.ps = stats.ps();
            Serial.println(displayManager.ps);
#endif
        }
        static long statiscs = 0;
        if (millis() - statiscs > Config::STATUS_INTERVAL - 100)
        {
            char value[5] = {0};
            snprintf(value, 5, "%d", lora.packetRssi());
            lora.send(0, "rssi", value, Config::TERMINAL_ID);
            statiscs = millis();
        }
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));
        lora.loop();
        bool hasMessage = lora.processIncoming(rec);

        if (!hasMessage)
        {
            return false;
        }
        Logger::info("Handled from: %d: %s|%s", rec.from, rec.event, rec.value);

#ifdef DISPLAY_ENABLED
        displayManager.showMessage("[RECV] (" + String(rec.id) + ") " + String(rec.event) + ": " + String(rec.value));
#endif

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

#endif