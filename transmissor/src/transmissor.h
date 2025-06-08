

#include "logger.h"
#include "logger.h"
#include "LoRa32.h"
#ifdef DISPLAY_ENABLED
#include "DisplayManager.h"
#endif

#ifdef WIFI
#include "wificonn.h"
#endif

#include "stats.h"

/// LoRa -------------------------------------------------------------------------

class App
{
private:
    uint8_t terminalId;
    LoRa32 lora;

public:
    void initNet()
    {
#ifdef WIFI
        wifiConn.setup();
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
    }

    void loop()
    {
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
            displayManager.termTotal = deviceInfo.count();
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
        else
        {
            Logger::info("Received message from: %d event: %s|%s", rec.from, rec.event, rec.value);
        }
    }
};

static App app;
