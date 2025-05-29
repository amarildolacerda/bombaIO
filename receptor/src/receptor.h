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
    bool loraConnected = false;
    const String terminalName = Config::TERMINAL_NAME;
    const uint8_t terminalId = Config::TERMINAL_ID;

public:
    void initLora()
    {
        loraConnected = lora.begin(terminalId, Config::BAND, Config::PROMISCUOS);
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
        lora.loop();
        if (lora.available())
        {
            ultimoReceived = millis();
            char buf[Config::MESSAGE_MAX_LEN];
            memset(buf, 0, sizeof(buf));
            uint8_t len = sizeof(buf);
            if (lora.receiveMessage(buf, &len))
            {
                handleMessage(buf);
            }
        }
        else if (millis() - sendUpdated > 100)
        {
            sendUpdated = millis();
            if (lora.connected)
            {
                if (millis() - statusUpdater > Config::STATUS_INTERVAL)
                {
                    setStatusChanged(true);
                    statusUpdater = millis();
                }
            }
            if (millis() - ultimoReceived > 60000)
            {
                initLora();
            }
        }
    }
    void sendEvent(uint8_t tid, String event, String value)
    {
        lora.send(tid, event, value);
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

    bool handleMessage(char *message)
    {
        uint8_t tfrom = lora.headerFrom();
        bool handled = false;
        if (strcmp(message, "ack") == 0)
            return true;
        if (strcmp(message, "nak") == 0)
            return false;
        if (strstr(message, "|") == NULL)
            return false;

        char buffer[sizeof(message)];
        strcpy(buffer, message);
        // Divide a string usando '|' como delimitador
        const char *event = strtok(buffer, "|"); // Pega a primeira parte ("status")
        const char *value = strtok(NULL, "|");   // Pega a segunda parte ("value")

        if ((event == NULL) || (value == NULL))
            return false;

        if (strcmp(event, "status") == 0)
        {
            // sendStatus(0, "get.status");
            setStatusChanged(true);
            return true;
        }
        else if (strcmp(event, "presentation") == 0)
        {

            lora.send(0, "presentation", terminalName);
            return true;
        }
        else if (strcmp(event, "reset") == 0)
        {
            // resetFunc();
            return true;
        }
        else if (strcmp(event, "gpio") == 0)
        {
            if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0)
            {
                digitalWrite(Config::RELAY_PIN, (strcmp(value, "on") == 0) ? HIGH : LOW);
                handled = true;
            }
            else if (strcmp(value, "toggle") == 0)
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                handled = true;
            }
            setStatusChanged(true);
        }

        ack(tfrom, handled);
        return handled;
    }
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? "ack" : "nak", "");
    }
    void setStatusChanged(bool b)
    {
        lora.send(0, "status", b ? "on" : "off");
    }
};

App app;

void triggerCallback()
{
    app.savePinState(digitalRead(Config::RELAY_PIN));
    app.setStatusChanged(true);
    systemState.pinStateChanged = true;
}

void initCallback()
{
    detachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN));
    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), triggerCallback, CHANGE);
}

//------------------------------------------------------app