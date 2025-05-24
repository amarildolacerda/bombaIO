#include "Arduino.h"
#include "config.h"
#include "RH_RF95.h"
#include <SoftwareSerial.h>

#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL      // 10 segundos para zerar contagem
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL // 1 minuto para 3 boots

//--------------------------------------------------LOG
enum class LogLevel
{
    ERROR,
    WARNING,
    RECEIVE,
    SEND,
    INFO,
    DEBUG,
    VERBOSE
};
class Logger
{
public:
    static void info(String msg)
    {
        log(LogLevel::INFO, msg);
    }
    static void error(String msg)
    {
        log(LogLevel::ERROR, msg);
    }
    static bool log(const LogLevel level, const String msg)
    {
        static const char levelStrings[][7] PROGMEM = {
            "[ERRO]", "[WARN]", "[RECV]", "[SEND]", "[INFO]",
            "[DBUG]",
            "[VERB]"};

        static const char colorCodes[][8] PROGMEM = {
            "\033[31m", // ERRO - Red
            "\033[34m", // WARN - Blue
            "\033[35m", // RECEIVE - Magenta
            "\033[36m", // SEND - Cyan
            "\033[32m", // INFO - Green
            "\033[33m", // DBUG - Yellow
            "\033[37m"  // VERB - White
        };

        int idx = static_cast<int>(level);
        char levelBuffer[7];
        char colorBuffer[8];
        strcpy_P(levelBuffer, levelStrings[idx]);
        strcpy_P(colorBuffer, colorCodes[idx]);

        Serial.print(colorBuffer);
        Serial.print(levelBuffer);
#ifdef TIMESTAMP
//        Serial.print(F(" "));
//        Serial.print(getISOHour());
#endif
        Serial.print(F("-"));

        Serial.println(msg);
        Serial.print(F("\033[0m")); // Reset color if used
        return true;
    }
};

//--------------------------------------------------RF
SoftwareSerial RFSerial(Config::RX_PIN, Config::TX_PIN); // RX, TX

class LoraRF
{
private:
    RH_RF95<decltype(RFSerial)> rf95;
    uint8_t tId = 1;
    bool _promiscuos = false;
    uint8_t _hto = 0xFF;
    uint8_t _hfrom = 0xFF;
    uint8_t msgId = 0;

public:
    LoraRF() : rf95(RFSerial) {}
    bool begin(uint8_t terminalId, long band, bool promisc = true)
    {
        tId = terminalId;
        RFSerial.begin(Config::LORA_SPEED);
        bool result = rf95.init();
        _promiscuos = promisc;
        rf95.setPromiscuous(true); // ajusta situaçoes que não reconhece
        rf95.setFrequency(band);
        return result;
    }
    bool loop()
    {
        return true;
    }
    bool receive(char *buffer, uint8_t *len)
    {
        bool result = false;
        if (rf95.waitAvailableTimeout(100))
        {
            result = rf95.recv((uint8_t *)buffer, len);
            if (result)
            {
                _hto = rf95.headerTo();
                _hfrom = rf95.headerFrom();
                if (_hfrom == tId)
                    return false;
                if ((_hto != 0xFF) && (!_promiscuos) && (_hto != tId))
                {
                    return false;
                }
                Logger::log(LogLevel::RECEIVE, "From: " + (String)_hfrom + " " + (String)buffer);
            }
        }
        return result;
    }
    uint8_t headerTo()
    {
        return rf95.headerTo();
    }
    uint8_t headerFrom()
    {
        return rf95.headerFrom();
    }
    bool send(uint8_t tTo, String buffer)
    {
        return send(tTo, (char *)buffer.c_str(), buffer.length());
    }
    bool send(uint8_t tTo, char *buffer, uint8_t len)
    {
        bool result = false;
        rf95.setModeTx();
        rf95.setHeaderFrom(tId);
        rf95.setHeaderTo(tTo);
        rf95.setHeaderId(msgId++);
        rf95.setHeaderFlags(len, 0xFF);
        if (rf95.send((uint8_t *)buffer, len))
        {
            result = rf95.waitPacketSent(1000);
            delay(10);
        };

        rf95.setModeRx();
        Logger::log(LogLevel::SEND, "To: " + (String)tTo + " " + (String)buffer);
        return result;
    }
    bool available()
    {
        return rf95.available();
    }
};
static LoraRF lora;

//-------------------------------------------------Setup
class App
{
private:
    long statusUpdater = 0;
    bool loraConnected = false;
    const String terminalName = Config::TERMINAL_NAME;

public:
    void begin(long speed)
    {
        Serial.begin(speed);
        while (!Serial)
            ;
        loraConnected = lora.begin(Config::TERMINAL_ID, Config::BAND, Config::PROMISCUOS);
        if (!loraConnected)
        {
            // Logger::info("LoRa nao conectou");
            Serial.print("LoRa nao iniciou");
        };
        pinMode(Config::LED_PIN, OUTPUT);
        pinMode(Config::RELAY_PIN, OUTPUT);
        initPinRelay();
    }
    void loop()
    {
        lora.loop();
        if (lora.available())
        {
            char buf[Config::MESSAGE_MAX_LEN] = {0};
            uint8_t len = sizeof(buf);
            if (lora.receive(buf, &len))
            {
                handleMessage(buf);
            }
        }
        else
        {
            if (millis() - statusUpdater > Config::STATUS_INTERVAL)
            {
                sendStatus(0);
            }
        }
    }
    void sendEvent(uint8_t tid, String event, String value)
    {
        String msg = event + "|" + value;
        lora.send(tid, (char *)msg.c_str(), msg.length());
    }
    void sendStatus(uint8_t tid)
    {
        sendEvent(tid, "status", digitalRead(Config::RELAY_PIN) ? "on" : "off");
        statusUpdater = millis();
    }
    void (*resetFunc)(void) = 0;

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
        pinMode(5, OUTPUT);
        bool savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);
        Logger::info(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF");
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
            sendStatus(tfrom);
            return true;
        }
        else if (strcmp(event, "presentation") == 0)
        {

            sendEvent(tfrom, "presentation", terminalName);
            return true;
        }
        else if (strcmp(event, "reset") == 0)
        {
            resetFunc();
            return true;
        }
        else if (strcmp(event, "gpio") == 0)
        {
            if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0)
            {
                digitalWrite(Config::RELAY_PIN, (strcmp(value, "on") == 0) ? HIGH : LOW);

                sendStatus(tfrom);
                savePinState(strcmp(value, "on") == 0);
                handled = true;
            }
            else if (strcmp(value, "toggle") == 0)
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                systemState.pinStateChanged = true;
                sendStatus(tfrom);
                savePinState(!currentState);
                handled = true;
            }
        }
        else if (strcmp(event, "ping") == 0)
        {
            lora.send(tfrom, "pong");
            // nao responde com ack nem nak
            return true;
        }
        else if (strcmp(event, "time") == 0)
        {
            handled = true;
#ifdef TIMESTAMP

            // if (timestamp.isValidTimeFormat(value))
            // {
            //     timestamp.setCurrentTime(value);
            //     handled = true;
            // }
            // else
            // {
            //     Serial.println(F("Erro: Timestamp formato de tempo inválido"));
            // }
#endif
        }

        ack(tfrom, handled);
        return handled;
    }
    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, "ack");
    }
};

App app;

//------------------------------------------------------app
