#ifndef DISPLAY_HELTEC_H
#define DISPLAY_HELTEC_H

#include "DisplayInterface.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// Configurações para Heltec ESP32 V2
#define OLED_ADDRESS 0x3C
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

class DisplayHeltec : public DisplayInterface
{
private:
    SSD1306Wire display;
    uint16_t currentX = 0;
    uint16_t currentY = 0;
    uint8_t textSize = 1;

public:
    const float rowHeight = 8.5;
    float colWidth = 7;
    DisplayHeltec() : display(OLED_ADDRESS, 400000, OLED_SDA, OLED_SCL, GEOMETRY_128_64, OLED_RST) {}

    bool initialize() override
    {
        // Reset físico do display
        pinMode(OLED_RST, OUTPUT);
        digitalWrite(OLED_RST, LOW);
        delay(50);
        digitalWrite(OLED_RST, HIGH);
        delay(50);

        // Inicialização do display
        if (!display.init())
        {
            return false;
        }

        display.displayOn();
        display.flipScreenVertically();
        setTextSize(1); // Tamanho padrão
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.clear();
        colWidth = 128 / 20;
        display.display();
        return true;
    }

    void setPos(uint8_t linha, uint8_t coluna) override
    {
        currentX = (coluna * colWidth * textSize) + 2;
        currentY = linha * rowHeight * textSize;
    }
    void print(const String msg) override
    {
        /*Serial.print("Row: ");
        Serial.print(currentY);
        Serial.print(" Col: ");
        Serial.print(currentX);
        Serial.print(" ");
        Serial.println(msg);
        */
        display.drawString(currentX, currentY, msg);
        currentX += display.getStringWidth(msg);
    }

    void println(const String msg) override
    {
        print(msg);
        currentX = 0;
        currentY += rowHeight * textSize;
        if (currentY > 63)
            currentY = 0; // Limite do display
    }

    void clearDisplay() override
    {
        display.clear();
        currentX = 0;
        currentY = 0;
    }

    void show() override
    {
        display.display();
    }

    void setTextColor(uint8_t color, uint8_t bgColor = BLACK) override
    {
        display.setColor(color == WHITE ? WHITE : BLACK);
    }
    void setColor(uint8_t color) override
    {
        display.setColor(color == WHITE ? WHITE : BLACK);
    }
    void fillRect(int x1, int y1, int x2, int y2) override
    {
        display.fillRect(x1, y1, x2, y2);
    }
    void setTextSize(uint8_t size) override
    {
        textSize = size;
        switch (size)
        {
        case 1:
            display.setFont(ArialMT_Plain_10);
            break;
        case 2:
            display.setFont(ArialMT_Plain_16);
            break;
        case 3:
            display.setFont(ArialMT_Plain_24);
            break;
        default:
            display.setFont(ArialMT_Plain_10);
        }
    }

    // Métodos adicionais úteis
    void flipScreen(bool flip)
    {
        if (flip)
        {
            display.flipScreenVertically();
        }
        else
        {
            display.flipScreenVertically();
        }
    }

    void setContrast(uint8_t contrast)
    {
        display.setContrast(contrast);
    }
};

#endif