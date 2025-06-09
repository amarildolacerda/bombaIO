

#include "logger.h"
#include "logger.h"
#ifdef DISPLAY_ENABLED
#include "DisplayManager.h"
#endif

#ifdef WIFI
#include "wificonn.h"
#endif

#include "stats.h"
#include "alexaCom.h"
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
            lora.send(dev.tid, EVT_GPIO, state ? GPIO_ON : GPIO_OFF, Config::TERMINAL_ID);

            break;
        }
    }
#endif
}

class App
{
private:
    uint8_t terminalId;

public:
    void initNet()
    {
#ifdef WIFI
        wifiConn.setup(&alexaDeviceCallback);
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

        terminalId = Config::TERMINAL_ID;

        Serial.println("LoRa Duplex");

        lora.begin(terminalId, Config::LORA_BAND, true); // initialize LoRa at 868 MHz

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

        // Ping --------------------------------------------------------------
        static long lastSendTime = 0; // last send time
        if (millis() - lastSendTime > Config::PING_TIMEOUT_MS)
        {
            sendPing();
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
                lora.send(0xFF, EVT_PRESENTATION, Config::TERMINAL_NAME, terminalId);
                discUpdate = millis();
            }
        }
        updateDisplay();
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
        lora.send(0xFF, EVT_PING, Config::TERMINAL_NAME, terminalId);
    }
    void ackNak(uint8_t to, bool b)
    {
        lora.send(to, b ? EVT_ACK : EVT_NAK, Config::TERMINAL_NAME, terminalId);
    }
    void executeStatus(const MessageRec rec)
    {
        // gerar historico
        // notificar alexa

        bool status = strcmp(rec.value, "on") == 0;
        deviceInfo.updateState(rec.from, status);
        if (rec.from != 0xFF)
            alexaCom.addDevice(rec.from, String(rec.from).c_str());

        alexaCom.updateStateAlexa(String(rec.from).c_str(), String(status ? "on" : "off").c_str());
    }
    void handleReceived(MessageRec &rec)
    {
        Logger::info("Handled from: %d event: %s|%s", rec.from, rec.event, rec.value);
        stats.rxSuccess++;
        if (rec.event == EVT_PING)
        {
            lora.send(rec.from, EVT_PONG, Config::TERMINAL_NAME, terminalId);
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
