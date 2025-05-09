#include "display_manager.h"

#include "config.h"
#include "system_state.h"
#include "logger.h"

#ifdef TTGO
#include "LoRa.h"
#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
Adafruit_SSD1306 display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1);
#endif

namespace DisplayManager
{
    void initializeDisplay()
    {
#ifdef TTGO
        display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.display();
#endif
    }

#ifdef TTGO
    static uint32_t ultimoBaixo = 0; // Variável para armazenar o último tempo que o sinal foi baixo}

#endif
    void updateDisplay()
    {

#ifdef TTGO
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
#endif
    }

    void clearDisplay()
    {
#ifdef TTGO

        display.clearDisplay();
        display.display();
#endif
    }

    void displayMessage(const String &message)
    {
#ifdef TTGO
        display.setCursor(0, 0);
        display.println("Mensagem: " + message);
        display.display();
#endif
    }

    void displayStatus(const String &status)
    {
#ifdef TTGO

        display.setCursor(0, 10);
        display.println("Status: " + status);
        display.display();
#endif
    }

    void displayLoraEvent(const String &event, const String &value)
    {
#ifdef TTGO

        display.setCursor(0, 20);
        display.println("Evento LoRa: " + event);
        display.println("Valor LoRa: " + value);
        display.display();
#endif
    }

    void displayLoraRcvMessage(const String &message)
    {
#ifdef TTGO

        display.setCursor(0, 30);
        display.println("RCV: " + message);
        display.display();
#endif
    }

    void displayTime(const String &time)
    {
#ifdef TTGO

        display.setCursor(0, 40);
        display.println("Hora: " + time);
        display.display();
#endif
    }

    void displayLowQualityLink(int rssi, float snr)
    {
#ifdef TTGO

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
#endif
    }

    void eventEnviado(const String &event)
    {
#ifdef TTGO

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("enviado:");
        display.println(event);
        display.display();
#endif
    }
}
