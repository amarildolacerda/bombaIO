#ifndef TTGO_DISPLAY_H
#define TTGO_DISPLAY_H

#ifdef DISPLAYTTGO
#include "DisplayInterface.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "logger.h"

#define OLED_RST 16 // Pino de reset (para TTGO)
static Adafruit_SSD1306 displayTtgo(128, 64, &Wire, OLED_RST);
//static Adafruit_SSD1306 displayTtgo;

class TtgoDisplay : public DisplayInterface
{
private:
    const long rowHeight = 9.30; // Config::SCREEN_HEIGHT / 7;
    const long colWidth = 6.95;  // Config::SCREEN_WIDTH / 21;

public:
    void setPos(const uint8_t linha, const uint8_t coluna) override
    {
        displayTtgo.setCursor((coluna * (colWidth)) + 1, (linha * rowHeight) + (linha < 6 ? 1 : 2));
    }
    bool initialize() override
    {

        if (displayTtgo.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        {

            // Configurações originais de fábrica:
            displayTtgo.setRotation(0);                        // Orientação padrão
            displayTtgo.dim(false);                            // Brilho normal
            displayTtgo.setTextWrap(false);                    // Desativar quebra de texto
            displayTtgo.ssd1306_command(SSD1306_SETPRECHARGE); // Resetar timing
            displayTtgo.ssd1306_command(0xF1);

            displayTtgo.clearDisplay();
            displayTtgo.cp437(true);
            displayTtgo.setTextSize(1);
            displayTtgo.setTextColor(SSD1306_WHITE);
            displayTtgo.print("Carregando...");
            displayTtgo.display();
            return true;
        }
        return false;
    }

    void setTextColor(uint8_t color, uint8_t fundo)
    {
        displayTtgo.setTextColor(color, fundo);
    }
    void setTextSize(uint8_t tam)
    {
        displayTtgo.setTextSize(tam);
    }

    void print(const String msg) override
    {
        Serial.println(msg);
        displayTtgo.print(msg);
    }
    void println(const String msg)
    {
        displayTtgo.println(msg);
    }

    void clearDisplay() override
    {
        displayTtgo.clearDisplay();
    }

    void display() override
    {
        displayTtgo.display();
    }
};

#endif // TTGO
#endif // TTGO_DISPLAY_H