

#include "logger.h"
#include "logger.h"
#ifdef DISPLAY_ENABLED
#include "DisplayManager.h"
#endif

#ifdef WIFI
#include "WiFiConn.h"
#endif

#include "stats.h"

#ifdef ALEXA
#include "alexaCom.h"
#endif

#include "LoRaCom.h"
#include "SystemState.h"

#ifdef GATEWAY
#include "DeviceInfo.h"
#endif

/// LoRa -------------------------------------------------------------------------

static void alexaDeviceCallback(uint8_t tid, const char *device_name, bool state, unsigned char value)
{
#ifdef ALEXA
    LoRaCom::send(tid, EVT_GPIO, state ? GPIO_ON : GPIO_OFF);
#endif
}

class App
{
private:
public:
    void initNet()
    {
#ifdef WIFI
#ifdef ALEXA
        wifiConn.setup(alexaDeviceCallback);
#else
        wifiConn.setup();
#endif
#endif
    }
    void initPerif()
    {
#ifdef DISPLAY_ENABLED
        displayManager.initialize();
#endif
    }
    void setup()
    {
        Serial.begin(115200); // initialize serial
        while (!Serial)
            ;

        systemState.terminalId = Config::TERMINAL_ID;
        systemState.terminalName = String(Config::TERMINAL_NAME);
#ifdef GATEWAY
        systemState.isGateway = true;
#else
        systemState.isGateway = (systemState.terminalId == 0);
        if (!systemState.isGateway)
        {
            pinMode(Config::RELAY_PIN, OUTPUT);
        }
#endif

        Serial.println("LoRa Duplex");

        lora.begin(systemState.terminalId, Config::LORA_BAND, true); // initialize LoRa at 868 MHz

        initPerif();

        initNet();

        systemState.isInitialized = lora.connected;
        Serial.println("LoRa init succeeded.");

        systemState.setDiscovering(true);
    }

    long discoveryUpdate = 0;
    void loop()
    {
        systemState.handle();
        systemState.isRunning = true;
        systemState.previousMillis = millis();
#ifdef WIFI
        wifiConn.loop();
#endif

        // Lora ------------------------------------------------------------
        lora.loop();

        static long lastSendTime = 0; // last send time
        if (millis() - lastSendTime > Config::PING_TIMEOUT_MS)
        {
#ifdef GATEWAY
            sendPing();
#else
            sendStatus();
#endif
            lastSendTime = millis(); // timestamp the message
        }
        static long receiveUpdate = 0;
        MessageRec rec;
        if (lora.processIncoming(rec))
        {
            handleReceived(rec);
        }

        if (systemState.isDiscovering)
        {
            static long discUpdate = 0;
            if (millis() - discUpdate > 10000)
            {
                lora.send(0xFF, EVT_PRESENTATION, systemState.terminalName.c_str(), systemState.terminalId);
                discUpdate = millis();
            }
        }
        updateDisplay();
        systemState.isRunning = false;
    }

    void updateDisplay()
    {
// Display -----------------------------------------------------------
#ifdef DISPLAY_ENABLED
        static long displayUpdate = 0;
        if (millis() - displayUpdate > 1000)
        {
#ifdef GATEWAY
            displayManager.termAtivos = deviceInfo.running();
            displayManager.termTotal = deviceInfo.size();
#endif
#ifdef WIFI
            displayManager.isoDateTime = wifiConn.getISOTime();
#endif
            displayManager.isDiscovering = systemState.isDiscovering;
            displayManager.snr = lora.packetSnr();
            displayManager.startedISODateTime = systemState.startedISODateTime;
            displayManager.rssi = lora.packetRssi();
            displayManager.ps = stats.ps();
            displayManager.loraConnected = systemState.isInitialized;
            displayManager.handle();
            displayUpdate = millis();
        }
#endif
    }

    //---------------------------
    // Ping
    // Envia um ping para o clientes
    // O clientes deve responder com um pong

    void sendPing()
    {
        lora.send(0xFF, EVT_PING, Config::TERMINAL_NAME, systemState.terminalId);
    }
    void sendStatus()
    {
        lora.send(0, EVT_STATUS, digitalRead(Config::RELAY_PIN) ? "on" : "off", systemState.terminalId);
    }
    void ackNak(uint8_t to, bool b)
    {
        lora.send(to, b ? EVT_ACK : EVT_NAK, Config::TERMINAL_NAME, systemState.terminalId);
    }
    void executeStatus(const MessageRec rec)
    {
        // gerar historico
        // notificar alexa

        bool status = strcmp(rec.value, "on") == 0;
#ifdef GATEWAY
        deviceInfo.updateState(rec.from, status);
#endif
#ifdef ALEXA
        if (rec.from != 0xFF)
        {
            if (alexaCom.indexOf(rec.from) < 0)
                alexaCom.addDevice(rec.from, String(rec.from).c_str());
            alexaCom.updateStateAlexa(rec.from, status);
        }
#endif
    }

    //------------------------------------------------
    void handleReceived(MessageRec &rec)
    {
#ifdef DISPLAY_ENABLED
        displayManager.showEvent(String(rec.from) + " " + String(rec.event) + "  " + String(rec.value));
#endif

#ifndef GATEWAY
        if (strcmp(rec.event, EVT_GPIO) == 0)
        {
            if (strcmp(rec.value, GPIO_ON) == 0)
            {
                digitalWrite(Config::RELAY_PIN, HIGH);
            }
            else if (strcmp(rec.value, GPIO_OFF) == 0)
            {
                digitalWrite(Config::RELAY_PIN, LOW);
            }
            else if (strcmp(rec.value, GPIO_TOGGLE) == 0)
            {
                digitalWrite(Config::RELAY_PIN, !digitalRead(Config::RELAY_PIN));
            }
            else
            {
                return;
            }
        }
#endif

        if (strcmp(rec.event, EVT_PING) == 0)
        {
            lora.send(rec.from, EVT_PONG, Config::TERMINAL_NAME, systemState.terminalId);
        }
        else if (strcmp(rec.event, EVT_ACK) == 0)
        {
            // Nada a fazer
        }
        else if (strcmp(rec.event, EVT_NAK) == 0)
        {
            // Nada a fazer
        }
        else if (strcmp(rec.event, EVT_PONG) == 0)
        {
            ackNak(rec.from, true);
        }
        else if (strcmp(rec.event, EVT_STATUS) == 0)
        {
            ackNak(rec.from, true);
            executeStatus(rec);
        }
        else if (strcmp(rec.event, EVT_PRESENTATION) == 0)
        {
            ackNak(rec.from, true);
#ifdef GATEWAY
            deviceInfo.updateDevice(rec.from, rec.value, false, lora.packetRssi());
            if (rec.from != 0xFF)
                alexaCom.addDevice(rec.from, String(rec.from).c_str());
#endif
        }
    }
};

static App app;
