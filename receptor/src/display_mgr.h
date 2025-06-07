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
            update();
        }
    }
    bool initialize()
    {
        return disp.initialize();
    }

    String ISOTime = "";
    void showFooter()
    {
        // int dispCount = DeviceInfo::deviceList.size();
        // uint8_t regCount = DeviceInfo::deviceRegList.size();
        // disp.setTextColor(WHITE, BLACK);
        // disp.fillRect(0, Config::SCREEN_HEIGHT - 9, Config::SCREEN_WIDTH,
        //               9, SSD1306_WHITE);
        // disp.setTextColor(BLACK, WHITE);
        disp.setPos(6, 0);
        // disp.print("D:");
        //  disp.print((String)dispCount);
        // disp.print("/");
        //  disp.print((String)regCount);
        disp.print("Id: ");
        disp.print((String)Config::TERMINAL_ID);
        disp.print("  ");
        disp.print(systemState.startedISODate.substring(0, 5));
        disp.setPos(6, 11);

        disp.print(ISOTime); // Mostra apenas HH:MM:SS
        // disp.setTextColor(WHITE, BLACK);
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
        disp.print("RxS: ");
        disp.print(String(stats.rxCount / millis() / 1000));

        disp.setPos(4, 0);
        disp.print(showEvent);

        showFooter();
        disp.show();
    }
};

static DisplayManager displayManager;

#endif
#endif