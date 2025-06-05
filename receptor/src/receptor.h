#include "Arduino.h"
#include "config.h"
#include "app_messages.h"

#ifdef __AVR__
#include "RH_RF95.h"
#include <SoftwareSerial.h>
#include "LoRaRF95.h"
#endif

#ifdef HELTEC
#include "heltec.h"
#include "LoRa32.h"
#ifdef WIFI
#include "WiFIManager.h"
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
#include "ws_logger.h"
#endif

#ifdef DISPLAY
#include "display_mgr.h"
#endif

#elif ESP32
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
    const char *terminalName = Config::TERMINAL_NAME;
    const uint8_t terminalId = Config::TERMINAL_ID;

public:
#if defined(WIFI)
    void initWiFi()
    {

        WiFi.mode(WIFI_STA);
        WiFiManager wifiManager;
        // wifiManager.resetSettings();
        wifiManager.setConnectTimeout(360);
        wifiManager.setConfigPortalTimeout(360);

        if (!wifiManager.autoConnect(Config::TERMINAL_NAME))
        {
            Logger::log(LogLevel::ERROR, F("Falha ao conectar WiFi - Reiniciando"));
        }

        delay(500); // Estabilização após conexão

        WSLogger::initWs(server);
        server.begin();
        Logger::info("Servidor Web iniciado na porta 80");

        //        systemState.setWifiStatus(true);

        char ipBuffer[16];

        snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
        Logger::log(LogLevel::INFO, ipBuffer);
    }
#endif

    //----------------------- NTP ----------------------
#if defined(WIFI)
    void initNTP()
    {
        configTzTime(Config::TIMEZONE, Config::NTP_SERVER);
        Logger::log(LogLevel::INFO, F("Sincronizando com NTP..."));

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 5000))
        { // Timeout de 5 segundos
            Logger::log(LogLevel::WARNING, F("Falha ao obter tempo NTP"));
            return;
        }
        else
        {
            systemState.startedISODate = systemState.getISOTime().substring(11, 18);
        }
    }
#endif

    void initLora()
    {
        lora.begin(terminalId, Config::LORA_BAND, Config::PROMISCUOS);
    }

    long sendUpdated = 0;

    void setup()
    {
        Serial.begin(Config::SERIAL_SPEED);
        while (!Serial)
            ;

#ifdef WIFI
        initWiFi();
        initNTP();
        WSLogger::initWs(server);
#endif

        lora.config = LORA_MED;
        lora.begin(terminalId, Config::LORA_BAND, Config::PROMISCUOS);
        if (!lora.connected)
        {
            Serial.print(F("LoRa failed to start"));
        };
#ifdef DISPLAY
        displayManager.initialize();
        displayManager.loraConnected = lora.connected;
#endif

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

#ifdef DISPLAY
        displayManager.ISOTime = systemState.getISOTime("%H:%M:%S");
        displayManager.rssi = lora.packetRssi();
        displayManager.snr = lora.packetSnr();
        displayManager.handle();
#endif
    }

    void sendEvent(uint8_t tid, const char *event, const char *value)
    {
        CRITICAL_SECTION
        {
            lora.send(tid, event, value, terminalId);
#ifdef DISPLAY
            displayManager.showMessage("[SEND] " + String(event) + ": " + String(value));
#endif
        }
    }

    void savePinState(bool state)
    {
        CRITICAL_SECTION
        {
#ifdef __AVR__
            EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
#else
            EEPROM.write(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
            EEPROM.commit();
#endif
        }
    }

    bool readPinState()
    {
        uint8_t val = 0;
        CRITICAL_SECTION
        {
            val = EEPROM.read(EEPROM_ADDR_PIN5_STATE);
        }
        return val == 1;
    }

    void initPinRelay()
    {
        bool savedState = false;
        pinMode(Config::RELAY_PIN, OUTPUT);
        savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);

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

#ifdef DISPLAY
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
            // lora.send(0, EVT_PRESENTATION, (char *)terminalName, terminalId);
            sendEvent(0, EVT_PRESENTATION, (char *)terminalName);
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
        sendEvent(0, EVT_STATUS, digitalRead(Config::RELAY_PIN) ? GPIO_ON : GPIO_OFF);
    }
};

App app;

void triggerCallback()
{
    CRITICAL_SECTION
    {
        app.savePinState(digitalRead(Config::RELAY_PIN));

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