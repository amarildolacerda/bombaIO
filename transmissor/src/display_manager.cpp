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

void DisplayManager::updateDisplay()
{
    lastUpdate = millis();
    display.clearDisplay();
    display.setCursor(0, 0);

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
    display.print("Radio: ");
    display.print(loraInitialized ? (baixo) ? "Baixo" : "OK" : "FALHA");
    display.print(" (");
    display.print(rssi);
    display.println(")");

    display.print("Estado: ");
    display.println(relayState);

    if (loraRcvEvent.length() > 0)
    {
        display.print("Terminal: ");
        display.println(_tid);
        display.print("Evento: ");
        display.println(loraRcvEvent);
        display.print("Value: ");
        display.println(loraRcvValue);
    }
    else
    {
        display.println("Evento: NENHUM");
    }
    showFooter();
    display.display();
}
void DisplayManager::showFooter()
{
    dispCount = DeviceInfo::deviceList.size();
    uint8_t regCount = DeviceInfo::deviceRegList.size();
    display.setTextColor(SSD1306_WHITE);
    display.fillRect(0, Config::SCREEN_HEIGHT - 10, Config::SCREEN_WIDTH,
                     10, SSD1306_WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(0, Config::SCREEN_HEIGHT - 8);
    display.print("D:");
    display.print(dispCount);
    display.print("/");
    display.print(regCount);
    display.print(" v:");
    display.print(ver);
    display.setCursor(Config::SCREEN_WIDTH - 49, Config::SCREEN_HEIGHT - 8);
    display.println(DeviceInfo::getISOTime().substring(11, 19)); // Mostra apenas HH:MM:SS
    display.setTextColor(WHITE, BLACK);
}

void DisplayManager::handle()
{
    if (millis() - lastUpdate > 1000)
        updateDisplay();
}

void DisplayManager::message(const String &event)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Enviou:");
    display.println(event);
    showFooter();
    display.display();
    lastUpdate = millis();
}

DisplayManager displayManager;

#endif