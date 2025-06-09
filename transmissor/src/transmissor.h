

#include "logger.h"
#include "logger.h"
#ifdef DISPLAY_ENABLED
#include "DisplayManager.h"
#endif

#ifdef WIFI
#include "wificonn.h"
#endif

#include "stats.h"

#ifdef ALEXA
#include "alexaCom.h"
#endif

#include "loraCom.h"

/// LoRa -------------------------------------------------------------------------

static void alexaDeviceCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
#ifdef ALEXA
    // const uint8_t alexaId = (uint8_t)device_id;
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "Callback da Alexa: %s", device_name);
    Logger::log(LogLevel::DEBUG, logMsg);

    for (auto &dev : alexaCom.alexaDevices)
    {
        if (dev.uniqueName().equals(device_name))
        {
            snprintf(logMsg, sizeof(logMsg), "Alexa(%d): %s command: %s",
                     dev.alexaId, dev.uniqueName().c_str(), state ? "ON" : "OFF");
            Logger::info(logMsg);

            // LoRaCom::sendCommand(EVT_GPIO, state ? GPIO_ON : GPIO_OFF, dev.tid);
            LoRaCom::send(dev.tid, EVT_GPIO, state ? GPIO_ON : GPIO_OFF);

            break;
        }
    }
#endif
}

class App
{
private:
public:
    void initNet()
    {
#ifdef WIFI
        wifiConn.setup(alexaDeviceCallback);
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

        systemState.isInitialized = true;
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

#ifdef GATEWAY
        // Ping --------------------------------------------------------------
        static long lastSendTime = 0; // last send time
        if (millis() - lastSendTime > Config::PING_TIMEOUT_MS)
        {
            sendPing();
            lastSendTime = millis(); // timestamp the message
        }
#endif
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
            displayManager.isDiscovering = systemState.isDiscovering;
            displayManager.termAtivos = deviceInfo.running();
            displayManager.termTotal = deviceInfo.size();
            displayManager.snr = lora.packetSnr();
            displayManager.startedISODateTime = systemState.startedISODateTime;
            displayManager.rssi = lora.packetRssi();
            displayManager.isoDateTime = wifiConn.getISOTime();
            displayManager.ps = stats.ps();
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
    void ackNak(uint8_t to, bool b)
    {
        lora.send(to, b ? EVT_ACK : EVT_NAK, Config::TERMINAL_NAME, systemState.terminalId);
    }
    void executeStatus(const MessageRec rec)
    {
        // gerar historico
        // notificar alexa

        bool status = strcmp(rec.value, "on") == 0;
        deviceInfo.updateState(rec.from, status);
#ifdef ALEXA
        if (rec.from != 0xFF)
        {
            if (alexaCom.indexOf(rec.from) < 0)
                alexaCom.addDevice(rec.from, String(rec.from).c_str());
            alexaCom.updateStateAlexa(rec.from, status);
        }
#endif
    }
    void handleReceived(MessageRec &rec)
    {
        Logger::info("Handled from: %d event: %s|%s", rec.from, rec.event, rec.value);

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
                {
                    Logger::error("GPIO command not recognized: %s", rec.value);
                }
                return;
            }

#endif
            if (rec.event == EVT_PING)
            {
                lora.send(rec.from, EVT_PONG, Config::TERMINAL_NAME, systemState.terminalId);
            }
            else if (rec.event == EVT_ACK)
            {
            }
            else if (rec.event == EVT_NAK)
            {
            }
            else if (rec.event == EVT_PONG)
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
                deviceInfo.updateDevice(rec.from, rec.value, false, lora.packetRssi());
                if (rec.from != 0xFF)
                    alexaCom.addDevice(rec.from, String(rec.from).c_str());
            }
            else
            {
                Logger::info("Received message from: %d event: %s|%s", rec.from, rec.event, rec.value);
            }
        }
    };

    static App app;
