#ifndef DISPLAY_MGR_H
#define DISPLAY_MGR_H

#ifdef DISPLAY_ENABLED

// #include "device_info.h"
#include "queue_message.h"
#ifdef WIFI
#include <WiFi.h>
#endif
// #include "system_state.h"
#ifdef DISPLAYTTGO
#include <DisplayTtgo.h>
static DisplayTTGO disp;
#endif
#ifdef DISPLAYHELTEC
#include "DisplayHeltec.h"
static DisplayHeltec disp;
#endif

#include "stats.h"

class DisplayManager
{
private:
    unsigned long updated = 0;
    String showEvent = "";
    long ps = 0;

public:
    int snr = 0;
    int rssi = 0;
    void setVersion(const String ver)
    {
    }
    void showMessage(const String event)
    {
        showEvent = event;
    }
    void handle()
    {
        if (millis() - updated > 1000)
        {
            ps = stats.ps();
            update();
            updated = millis();
        }
    }
    bool initialize()
    {
        return disp.initialize();
    }

    String ISOTime = "";
    void showFooter()
    {
        disp.setPos(6, 0);
        disp.print("Id: ");
        disp.print((String)Config::TERMINAL_ID);
        disp.print("  ");
        disp.print(systemState.startedISODate.substring(0, 5));
        disp.setPos(6, 11);
        disp.print(ISOTime); // Mostra apenas HH:MM:SS
    }
    bool loraConnected = false;
    String loraRcvEvent;
    void update()

    {
        disp.clearDisplay();
        disp.setPos(0, 0);

        disp.print("WiFi: ");
        disp.println(WiFi.isConnected() ? WiFi.localIP().toString() : "DESCONECTADO");

        static bool baixo = false;
        static uint32_t ultimoBaixo = 0;
        if (millis() - ultimoBaixo > 10000)
        {
            baixo = (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD);
            //      // Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
            ultimoBaixo = millis();
        }
        disp.setPos(1, 0);
        disp.print("Radio: ");
        disp.print(loraConnected ? (baixo) ? "Baixo" : "OK" : "FALHA");
        disp.print(" ");
        if (!baixo)
        {
            disp.setPos(1, 13);
            disp.println(systemState.lastUpdateTime);
        }
        else
        {
            disp.println((String)rssi);
        }

        disp.setPos(2, 0);
        disp.print("Rx:  ");
        disp.print(String(ps));
        disp.print("ps");

        disp.setPos(4, 0);
        disp.print(showEvent);

        showFooter();
        disp.show();
    }
};

static DisplayManager displayManager;

#endif
#endif