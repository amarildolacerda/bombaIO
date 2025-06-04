#ifndef HELTEC_DISPLAY_H
#define HELTEC_DISPLAY_H

#ifdef DISPLAYHELTEC
#include "DisplayInterface.h"
#include "config.h"
#include "logger.h"
#include <Wire.h>
#include "heltec.h"

#include "DisplayInterface.h"

// # static htdisplay() Heltec.display

// #include "HT_SSD1306Wire.h"

// #include "OLEDDisplay->h"
//  #include "images.h"

// static SSD1306Wire htdisplay(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

class HeltecDisplay : public DisplayInterface
{
private:
    bool initialize() override
    {
        return true;
    }
    void setPos(const uint8_t row, const uint8_t col) override {}
    void print(const String msg) override {}
    void println(const String msg) override {}
    void clearDisplay() override {}
    void display() override {}
    void setTextColor(uint8_t color, uint8_t fundo) override {}
    void setTextSize(uint8_t tam) override {}
};

#endif // HELTEC
#endif // HELTEC_DISPLAY_H