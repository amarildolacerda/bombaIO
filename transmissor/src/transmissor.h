#ifndef TRANSMISSOR_H
#define TRANSMISSOR_H

#include <TuyaWifi.h>
#include <ArduinoJson.h>
#ifdef ESP32
#include "display_manager.h"
#elif ESP8266
#include "ESP8266WebServer.h"
#include "ESP8266WiFi.h"
#include "ESP8266httpUpdate.h"
#endif

#ifndef __AVR__
#include <WiFiManager.h>
#include <time.h>
#endif
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