#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include <LoRa.h>
#include <WiFi.h>
#include "system_state.h"

extern Adafruit_SSD1306 display;

namespace DisplayManager
{
    void initializeDisplay();
    void updateDisplay();
    void clearDisplay();
    void displayMessage(const String &message);
    void displayStatus(const String &status);
    void displayLoraEvent(const String &event, const String &value);
    void displayLoraRcvMessage(const String &message);
    void displayTime(const String &time);
    void displayLowQualityLink(int rssi, float snr);
    void eventEnviado(const String &event);

}

#endif // DISPLAY_MANAGER_H