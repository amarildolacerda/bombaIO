#ifndef TTGO_DISPLAY_H
#define TTGO_DISPLAY_H

#ifdef DISPLAYTTGO
#include "DisplayInterface.h"
#include <Wire.h>
#include "logger.h"

#define OLED_RST 16 // Pino de reset (para TTGO)
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
static Adafruit_SSD1306 display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1);

class DisplayTTGO : public DisplayInterface
{
private:
    const long rowHeight = 9.30; // Config::SCREEN_HEIGHT / 7;
    const long colWidth = 6.95;  // Config::SCREEN_WIDTH / 21;

public:
    void setPos(const uint8_t linha, const uint8_t coluna) override
    {
        display.setCursor((coluna * (colWidth)) + 1, (linha * rowHeight) + (linha < 6 ? 1 : 2));
    }

    bool initialize() override
    {

        if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
        {

            display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.display();
            return true;
        }
        return false;
    }

    void setTextColor(uint8_t color, uint8_t fundo)
    {
        display.setTextColor(color, fundo);
    }
    void setTextSize(uint8_t tam)
    {
        display.setTextSize(tam);
    }

    void print(const String msg) override
    {
        // Serial.println(msg);
        display.print(msg);
    }
    void println(const String msg)
    {
        display.println(msg);
    }

    void clearDisplay() override
    {
        display.clearDisplay();
    }

    void show() override
    {
        display.display();
    }
};

#endif // TTGO
#endif // TTGO_DISPLAY_H