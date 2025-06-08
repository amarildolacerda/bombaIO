// Compiler: The project is compiled with the ESP-IDF toolchain for ESP32/ESP8266.
// Platform: The project is designed to run on the ESP32/ESP8266 platform.
#ifndef DISPPLAY_MANAGER_H
#define DISPPLAY_MANAGER_H

#ifdef DISPLAY_ENABLED

// #include "deviceinfo.h"
// #include "queue_message.h"
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#endif
#ifdef DISPLAYTTGO
#include <DisplayTtgo.h>
static DisplayTTGO disp;
#endif
#ifdef DISPLAYHELTEC
#include <DisplayHeltec.h>
static DisplayHeltec disp;
#endif

class DisplayManager
{
private:
    unsigned long updated = 0;

public:
    int termAtivos = 0;
    int termTotal = 0;
    long ps = 0;
    int rssi = 0;
    int snr = 0;
    String isoDateTime = "";
    String startedISODateTime = "";
    bool isDiscovering = false;

    void showMessage(const String event)
    {
        disp.setPos(5, 0);
        disp.print(event);
    }
    void handle()
    {
        if (millis() - updated > 1000)
        {

            _update();
            updated = millis();
        }
    }
    bool initialize()
    {
        return disp.initialize();
    }

private:
    void _showFooter()
    {
        disp.fillRect(0, Config::SCREEN_HEIGHT - 12, Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT);
        disp.setTextColor(BLACK, WHITE);
        disp.setPos(6, 0);
        disp.print("D:");
        disp.print((String)termAtivos);
        disp.print("/");
        disp.print((String)termTotal);
        disp.print("|");
        disp.print((String)ps);
        disp.print("ps");
        disp.print("|     ");
        disp.setPos(6, 13);
        disp.println(isoDateTime.substring(11, 19)); // Mostra apenas HH:MM:SS
        disp.setTextColor(WHITE, BLACK);
    }
    bool loraConnected = false;
    String loraRcvEvent;
    void _update()

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
            Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
            ultimoBaixo = millis();
        }
        disp.setPos(1, 0);
        disp.print("Radio: ");
        disp.print(loraConnected ? (baixo) ? "Baixo" : "OK" : "FALHA");
        disp.print(" ");
        if (!baixo)
        {
            disp.setPos(1, 13);
            disp.println(startedISODateTime.substring(11, 16)); // Mostra apenas HH:MM:SS
        }
        else
        {
            disp.println((String)rssi);
        }

        if (isDiscovering)
        {
            disp.setPos(3, 0);
            disp.println(F("  Modo pareamento"));
            disp.println(F("   em andamento")); // redundante, so esta preenchendo espaço no display
        }
        /*else */
        /*if (DeviceInfo::history.size() > 0)
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
        else*/
        {
            disp.setPos(4, 0);
            disp.println(F("Evento: NENHUM"));
        }
        _showFooter();
        disp.show();
    }
};

static DisplayManager displayManager;

#endif
#endif