#ifndef HELTEC_DISPLAY_H
#define HELTEC_DISPLAY_H
// #include <Wire.h>
// #include <SSD1306Wire.h>
#include "heltec.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

class DisplayManager
{
private:
    void VextON(void)
    {
        pinMode(Vext, OUTPUT);
        digitalWrite(Vext, LOW);
    }
    void VextOFF(void) // Vext default OFF
    {
        pinMode(Vext, OUTPUT);
        digitalWrite(Vext, HIGH);
    }

public:
    bool connected = false;
    bool init()
    {
        connected = display.init();

        if (!connected)
        {
            Serial.println(F("Display não encontrado"));
            return false;
        }

        VextON();
        display.setFont(ArialMT_Plain_10);
        display.clear();
        display.drawString(0, 10, "Hello world");
        display.setFont(ArialMT_Plain_24);
        display.display();
        delay(100);
        return connected;
    }
};
DisplayManager displayManager;

#endif