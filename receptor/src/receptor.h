#include "Arduino.h"
#include "config.h"
#include "RH_RF95.h"
#include <SoftwareSerial.h>
#include "logger.h"
#include "LoRaRF95.h"

#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL      // 10 segundos para zerar contagem
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL // 1 minuto para 3 boots

//-------------------------------------------------Setup
typedef void (*CallbackFunction)(); // Define a assinatura do callback
CallbackFunction callback;

void initCallback(); // Variável para armazenar o callback

class App
{
private:
    long statusUpdater = 0;
    const String terminalName = Config::TERMINAL_NAME;
    const uint8_t terminalId = Config::TERMINAL_ID;

public:
    void initLora()
    {
        lora.begin(terminalId, Config::BAND, Config::PROMISCUOS);
    }

    long sendUpdated = 0;
    void setup()
    {
        Serial.begin(Config::SERIAL_SPEED);
        while (!Serial)
            ;
        lora.begin(terminalId, Config::BAND, Config::PROMISCUOS);

        if (!lora.connected)
        {
            Serial.print(F("LoRa nao iniciou"));
        };
        pinMode(Config::LED_PIN, OUTPUT);
        pinMode(Config::RELAY_PIN, OUTPUT);
        initPinRelay();
    }
    long ultimoReceived = 0;
    void loop()
    {
        MessageRec rec;
        if (lora.processIncoming(rec))
        {
            /// processar as mensagens;
            handleMessage(rec);
            ultimoReceived = millis();
        }
        lora.loop();

        if (millis() - sendUpdated > 100)
        {
            sendUpdated = millis();
            if (lora.connected)
            {
                if (millis() - statusUpdater > Config::STATUS_INTERVAL)
                {
                    setStatusChanged();
                    statusUpdater = millis();
                }
            }
            if (millis() - ultimoReceived > 60000)
            {
                initLora();
            }
        }
    }

    void savePinState(bool state)
    {
#ifdef __AVR__
        EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
#else
        EEPROM.write(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
        EEPROM.commit(); // No ESP8266/ESP32 você PRECISA chamar commit() para salvar
#endif
    }

    // Função para ler o estado do pino 5 da EEPROM
    bool readPinState()
    {
        return EEPROM.read(EEPROM_ADDR_PIN5_STATE) == 1;
    }

    void initPinRelay()
    {
        pinMode(Config::RELAY_PIN, OUTPUT);
        bool savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);
        Logger::info(String(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF").c_str());
        initCallback();
    }

    bool handleMessage(MessageRec &message)
    {
        uint8_t tfrom = lora.headerFrom();
        bool handled = false;
        if (strcmp(message.event, "ack") == 0)
            return true;
        if (strcmp(message.event, "nak") == 0)
            return false;

        if ((message.event == NULL) || (message.value == NULL))
            return false;

        if (strcmp(message.event, "status") == 0)
        {
            setStatusChanged();
            return true;
        }
        else if (strcmp(message.event, "presentation") == 0)
        {

            lora.send(0, "presentation", terminalName);
            return true;
        }
        else if (strcmp(message.event, "reset") == 0)
        {
            // resetFunc();
            return true;
        }
        else if (strcmp(message.event, "gpio") == 0)
        {
            if (strcmp(message.value, "on") == 0 || strcmp(message.value, "off") == 0)
            {
                digitalWrite(Config::RELAY_PIN, (strcmp(message.value, "on") == 0) ? HIGH : LOW);
                handled = true;
            }
            else if (strcmp(message.value, "toggle") == 0)
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                handled = true;
            }
            setStatusChanged();
        }

        ack(tfrom, handled);
        return handled;
    }
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? "ack" : "nak", "");
    }
    void setStatusChanged()
    {
        lora.send(0, "status", digitalRead(Config::RELAY_PIN) ? "on" : "off");
    }
};

App app;

void triggerCallback()
{
    app.savePinState(digitalRead(Config::RELAY_PIN));
    app.setStatusChanged();
    systemState.pinStateChanged = true;
}

void initCallback()
{
    detachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN));
    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), triggerCallback, CHANGE);
}

//------------------------------------------------------app