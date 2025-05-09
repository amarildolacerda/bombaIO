#include "display_manager.h"

#include "config.h"
#include "system_state.h"
#include "logger.h"
#include "LoRa.h"
#include <WiFi.h>
Adafruit_SSD1306 display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1);

namespace DisplayManager
{
    void initializeDisplay()
    {
        display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.display();
    }

    static uint32_t ultimoBaixo = 0; // Variável para armazenar o último tempo que o sinal foi baixo}

    void updateDisplay()
    {

        display.clearDisplay();
        display.setCursor(0, 0);

        display.print("WiFi: ");
        display.println(systemState.wifiConnected ? WiFi.localIP().toString() : "DESCONECTADO");

        int rssi = LoRa.packetRssi();
        float snr = LoRa.packetSnr();

        bool baixo = (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD);
        if (baixo && (millis() - ultimoBaixo > 30000))
        {
            Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
            ultimoBaixo = millis();
        }

        display.print("Radio: ");
        display.print(systemState.loraInitialized ? (baixo) ? "Baixo" : "OK" : "FALHA");
        display.print(" (");
        display.print(rssi);
        display.println(")");

        display.print("Estado: ");
        display.println(systemState.relayState);

        display.print("Atualizado: ");
        display.println(systemState.getISOTime().substring(11, 19)); // Mostra apenas HH:MM:SS

        /* if (systemState.loraRcvEvent.length() > 0)
         {
             display.print("Evento: ");
             display.println(systemState.loraRcvEvent);
             display.print("Value: ");
             display.println(systemState.loraRcvValue);
         }
         else
         {
             display.print("Evento: NENHUM");
         }
             */

        display.display();
    }

    void clearDisplay()
    {
        display.clearDisplay();
        display.display();
    }

    void displayMessage(const String &message)
    {
        display.setCursor(0, 0);
        display.println("Mensagem: " + message);
        display.display();
    }

    void displayStatus(const String &status)
    {
        display.setCursor(0, 10);
        display.println("Status: " + status);
        display.display();
    }

    void displayLoraEvent(const String &event, const String &value)
    {
        display.setCursor(0, 20);
        display.println("Evento LoRa: " + event);
        display.println("Valor LoRa: " + value);
        display.display();
    }

    void displayLoraRcvMessage(const String &message)
    {
        display.setCursor(0, 30);
        display.println("RCV: " + message);
        display.display();
    }

    void displayTime(const String &time)
    {
        display.setCursor(0, 40);
        display.println("Hora: " + time);
        display.display();
    }

    void displayLowQualityLink(int rssi, float snr)
    {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.println("Link Quality Low");
        display.print("RSSI: ");
        display.println(rssi);
        display.print("SNR: ");
        display.println(snr);
        display.display();
    }
}
