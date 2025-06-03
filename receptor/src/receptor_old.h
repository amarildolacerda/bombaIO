#include "Arduino.h"
#include "config.h"

#ifdef __AVR__
#include "RH_RF95.h"
#include <SoftwareSerial.h>
#include "LoRaRF95.h"
#endif

#ifdef ESP32
#include "LoRa32.h"
#endif

#include "logger.h"
#include "queue_message.h"

#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL

typedef void (*CallbackFunction)();
CallbackFunction callback;

void initCallback();

class App
{
private:
    long statusUpdater = 0;
    bool loraConnected = false;
    const char *terminalName = Config::TERMINAL_NAME;
    const uint8_t terminalId = Config::TERMINAL_ID;

public:
    void initLora()
    {
        loraConnected = lora.begin(terminalId, Config::LORA_BAND, Config::PROMISCUOS);
    }

    long sendUpdated = 0;

    void setup()
    {
        Serial.begin(Config::SERIAL_SPEED);
        while (!Serial)
            ;

        lora.begin(terminalId, Config::LORA_BAND, Config::PROMISCUOS);
        if (!lora.connected)
        {
            Serial.print(F("LoRa failed to start"));
        };

        pinMode(Config::LED_PIN, OUTPUT);
        pinMode(Config::RELAY_PIN, OUTPUT);
        initPinRelay();
    }

    long ultimoReceived = 0;

    void loop()
    {
        lora.loop();
        if (handleMessage())
        {
            ultimoReceived = millis();
        }

        if (millis() - sendUpdated > 100)
        {
            // sendUpdated = millis();

            // if (lora.connected)
            // {
            if (millis() - statusUpdater > Config::STATUS_INTERVAL)
            {
                setStatusChanged();
                statusUpdater = millis();
            }
            //  }

            if (millis() - ultimoReceived > 60000)
            {
                // initLora();
                ultimoReceived = millis();
                Logger::info("Tempo de atividade: %lu segundos", millis() / 1000);
#ifdef ESP32
                Logger::debug("Memória livre: %d bytes", ESP.getFreeHeap());
#endif
            }
        }
    }

    void sendEvent(uint8_t tid, const char *event, const char *value)
    {
        noInterrupts();
        lora.send(tid, event, value, terminalId);
        interrupts();
    }

    void savePinState(bool state)
    {
        noInterrupts();
#ifdef __AVR__
        EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
#else
        EEPROM.write(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
        EEPROM.commit();
#endif
        interrupts();
    }

    bool readPinState()
    {
        noInterrupts();
        uint8_t val = EEPROM.read(EEPROM_ADDR_PIN5_STATE);
        interrupts();
        return val == 1;
    }

    void initPinRelay()
    {
        noInterrupts();
        pinMode(Config::RELAY_PIN, OUTPUT);
        bool savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);
        interrupts();

        Logger::info(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF");
#ifdef ESP32
        initCallback();
#endif
    }

    bool handleMessage()
    {
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

        if (strstr(rec.event, EVT_PRESENTATION))
        {

            lora.send(0, EVT_PRESENTATION, (char *)terminalName, terminalId);
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

    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? EVT_ACK : EVT_NAK, "", terminalId);
    }

    void setStatusChanged()
    {
        lora.send(0, EVT_STATUS, digitalRead(Config::RELAY_PIN) ? GPIO_ON : GPIO_OFF, terminalId);
    }
};

App app;

void triggerCallback()
{
    CRITICAL_SECTION
    {
        app.savePinState(digitalRead(Config::RELAY_PIN));
        interrupts();
        app.setStatusChanged();
        systemState.pinStateChanged = true;
    }
}

static bool callbackInstaled = false;
void initCallback()
{
    if (callbackInstaled)
    {
        callbackInstaled = true;
        detachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN));
    }
    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), triggerCallback, CHANGE);
}