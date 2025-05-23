#ifdef TTGO
#include "display_manager.h"

#include "config.h"
#include "device_info.h"
#include "logger.h"

#include "LoRa.h"
#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
Adafruit_SSD1306 display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1);
using namespace std;

void DisplayManager::initializeDisplay()
{
    display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

const long rowHeight = 9.30; // Config::SCREEN_HEIGHT / 7;
const long colWidth = 6.95;  // Config::SCREEN_WIDTH / 21;
void setPos(long linha, long coluna)
{
    display.setCursor((coluna * (colWidth)) + 1, (linha * rowHeight) + (linha < 6 ? 1 : 2));
}
void DisplayManager::updateDisplay()
{
    lastUpdate = millis();
    display.clearDisplay();
    setPos(0, 0);

    display.print("WiFi: ");
    display.println(wifiConnected ? WiFi.localIP().toString() : "DESCONECTADO");

    static bool baixo = false;
    static uint32_t ultimoBaixo = 0;
    if (millis() - ultimoBaixo > 10000)
    {
        baixo = (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD);
        Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
        ultimoBaixo = millis();
    }
    setPos(1, 0);
    display.print("Radio: ");
    display.print(loraInitialized ? (baixo) ? "Baixo" : "OK" : "FALHA");
    display.print(" ");
    if (!baixo)
    {
        setPos(1, 13);
        display.println(systemState.lastUpdateTime);
    }
    else
    {
        display.println(rssi);
    }
    display.println("");

    if (systemState.isDiscovering())
    {
        setPos(3, 0);
        display.println("  Modo pareamento");
        display.println("   em andamento"); // redundante, so esta preenchendo espaço no display
    }
    else if (loraRcvEvent.length() > 0)
    {
        uint8_t i = 2;
        for (const auto &d : DeviceInfo::history)
        {
            setPos(i, 0);
            display.print(d.tid);
            setPos(i, 3);
            display.print(d.name.substring(0, 11));
            setPos(i++, 15);
            display.print(d.value);
        }
        /** setPos(2, 0);
         display.print("Term: ");
         display.print(_tid);
         int id = DeviceInfo::indexOf(_tid);
         display.print(" ");
         display.println(DeviceInfo::deviceRegList[id].name);

         setPos(3, 0);
         display.print("Estado: ");

         display.println(relayState); // redundante, so esta preenchendo espaço no display

         setPos(4, 0);
         display.print("Evento: ");
         display.println(loraRcvEvent);
         */

        // setPos(5, 0);
        // display.print("Value: ");
        // display.print(loraRcvValue);
        // setPos(5, 13);
        // display.println(systemState.lastUpdateTime);
    }
    else
    {
        display.println("Evento: NENHUM");
    }
    showFooter();
    display.display();
    stateTimeout = 1000;
}
void DisplayManager::showFooter()
{
    dispCount = DeviceInfo::deviceList.size();
    uint8_t regCount = DeviceInfo::deviceRegList.size();
    display.setTextColor(SSD1306_WHITE);
    display.fillRect(0, Config::SCREEN_HEIGHT - 9, Config::SCREEN_WIDTH,
                     9, SSD1306_WHITE);
    display.setTextColor(BLACK, WHITE);
    // display.setCursor(0, Config::SCREEN_HEIGHT - 8);
    setPos(6, 0);
    display.print("D:");
    display.print(dispCount);
    display.print("/");
    display.print(regCount);
    display.print(" v:");
    display.print(ver);
    // display.setCursor(Config::SCREEN_WIDTH - 49, Config::SCREEN_HEIGHT - 8);
    setPos(6, 13);
    display.println(DeviceInfo::getISOTime().substring(11, 19)); // Mostra apenas HH:MM:SS
    display.setTextColor(WHITE, BLACK);
}

void DisplayManager::handle()
{
    if (millis() - lastUpdate > stateTimeout)
        updateDisplay();
}

void DisplayManager::message(const String &event)
{
    display.setTextColor(BLACK, WHITE);
    setPos(6, 0);
    display.print("                     ");
    setPos(6, 0);
    display.print(event);
    display.setTextColor(WHITE, BLACK);

    display.display();
    lastUpdate = millis();
    stateTimeout = 2000;
}

DisplayManager displayManager;

#endif