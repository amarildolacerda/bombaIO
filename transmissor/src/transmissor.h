

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

        Serial.println("LoRa init succeeded.");
    }

    void loop()
    {
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

// Display -----------------------------------------------------------
#ifdef DISPLAY_ENABLED
        static long displayUpdate = 0;
        if (millis() - displayUpdate > 1000)
        {
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

    void handleReceived(MessageRec &rec)
    {
        Logger.info("Handled from: %s event: %s|%s", rec.from, rec.event, rec.value);
    }
};

static App app;
