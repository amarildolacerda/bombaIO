// Compiler: The project is compiled with the ESP-IDF toolchain for ESP32/ESP8266.
// Platform: The project is designed to run on the ESP32/ESP8266 platform.
#ifndef DISPPLAY_MANAGER_H
#define DISPPLAY_MANAGER_H

#ifdef DISPLAY_ENABLED

#include "device_info.h"
#include "queue_message.h"
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#endif
#include "system_state.h"
#include "stats.h"
#ifdef DISPLAYTTGO
#include <DisplayTtgo.h>
static DisplayTTGO disp;
#endif

class DisplayManager
{
private:
    unsigned long updated = 0;
    long ps = 0;

public:
    int snr = 0;
    int rssi = 0;
    void setVersion(const String ver)
    {
    }
    void showMessage(const String event)
    {
        disp.setPos(5, 0);
        disp.print(event);
    }
    String isoDateTime = "";
    void handle()
    {
        if (millis() - updated > 1000)
        {
            isoDateTime = DeviceInfo::getISOTime();
            ps = stats.ps();
            update();
        }
    }
    bool initialize()
    {
        return disp.initialize();
    }
    void showFooter()
    {
        int dispCount = DeviceInfo::deviceList.size();
        uint8_t regCount = DeviceInfo::deviceRegList.size();
        disp.setTextColor(WHITE, BLACK);
        // disp.fillRect(0, Config::SCREEN_HEIGHT - 9, Config::SCREEN_WIDTH,
        //               9, SSD1306_WHITE);
        disp.setTextColor(BLACK, WHITE);
        disp.setPos(6, 0);
        disp.print("D:");
        disp.print((String)dispCount);
        disp.print("/");
        disp.print((String)regCount);
        disp.print("|");
        // disp.print(systemState.startedISODate.substring(0, 5));
        disp.print((String)ps);
        disp.print("ps");
        disp.print("|     ");
        disp.setPos(6, 13);
        disp.println(isoDateTime.substring(11, 19)); // Mostra apenas HH:MM:SS
        disp.setTextColor(WHITE, BLACK);
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
            // Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
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
        // disp.setPos(2, 0);
        // disp.print("Rxs: ");
        // disp.print(String(ps));
        // disp.print(" ps");

        /*   if (systemState.isDiscovering())
           {
               disp.setPos(3, 0);
               disp.println(F("  Modo pareamento"));
               disp.println(F("   em andamento")); // redundante, so esta preenchendo espaço no display
           }
        else */
        if (DeviceInfo::history.size() > 0)
        {
            uint8_t i = 2;
            for (const auto &d : DeviceInfo::history)
            {
                disp.setPos(i, 0);
                disp.print("                     ");
                disp.setPos(i, 0);
                disp.print((String)d.tid);
                disp.setPos(i, 3);
                disp.print(d.name.substring(0, 11));
                disp.setPos(i++, 15);
                disp.print(d.value);
                // Serial.print(d.value);
            }
        }
        else
        {
            disp.setPos(4, 0);
            disp.println(F("Evento: NENHUM"));
        }
        showFooter();
        disp.show();
    }
};

static DisplayManager displayManager;

#endif
#endif