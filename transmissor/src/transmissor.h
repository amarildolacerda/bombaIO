#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <TuyaWifi.h>
#include <ArduinoJson.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFiManager.h>
#include <time.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Update.h>
#include "display_manager.h"
#include "LoRaCom.h"

// ========== Callback Tuya ==========
unsigned char handleTuyaCommand(unsigned char dp_id, const unsigned char dp_data[], unsigned short dp_len);

// ========== Funções de Rede ==========
void initWiFi();
void initNTP();

// ========== Web Server ==========
void handleRootRequest();
void handleStateRequest();

// ========== Inicialização do Tuya ==========
void initTuya();

// ========== Setup e Loop Principais ==========
void setup();
void loop();

#endif // TRANSMISSOR_H