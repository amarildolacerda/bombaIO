#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"

#if defined(__AVR__)
#include <util/atomic.h>
#define CRITICAL_SECTION ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#elif defined(ESP32)
#define LoRaWAN_DEBUG_LEVEL 1 // Ou outro valor entre 0-4
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define CRITICAL_SECTION ;

// Versão 1: Usando portENTER_CRITICAL (recomendada para ESP32)
// #define CRITICAL_SECTION                                            \
//    for (portMUX_TYPE __tempMutex__ = portMUX_INITIALIZER_UNLOCKED; \
//         (portENTER_CRITICAL(&__tempMutex__), true);                \
//         portEXIT_CRITICAL(&__tempMutex__))

#else
#error "Plataforma não suportada"
#endif
namespace Config
{
#ifdef __AVR__
    constexpr char TERMINAL_NAME[] = RELAY_LORA_NAME;
#else
    constexpr char TERMINAL_NAME[] = "D1";
#endif

    constexpr bool PROMISCUOS = LORA_PROMISCUOS;
    constexpr int TERMINAL_ID = LORA_TERMINAL_ID;
    constexpr int LED_PIN = LED_BUILTIN;

#if defined(TTGO) || defined(HELTEC)
    constexpr uint32_t LORA_BAND = 868E6; // usado esp32 TTGO LoRa32 v1
#elif RF95
    constexpr uint32_t LORA_BAND = 868.0; // usado RF95
#endif

#ifndef VRELAY_PIN
    constexpr int RELAY_PIN = 4;
#else
    constexpr int RELAY_PIN = VRELAY_PIN;
#endif
#ifndef MIRX_PIN
    constexpr int RX_PIN = 6;
#else
    constexpr int RX_PIN = MIRX_PIN;
#endif
#ifndef MOTX_PIN
    constexpr int TX_PIN = 7;
#else
    constexpr int TX_PIN = MOTX_PIN;
#endif
    constexpr long LORA_SPEED = 9600;
    constexpr long SERIAL_SPEED = 115200;
    constexpr uint16_t LORA_SYNC_WORD = 0x12;

    constexpr long STATUS_INTERVAL = 5000;
    constexpr int MESSAGE_MAX_LEN = 128;
};

class SystemState
{
public:
    long previousMillis = 0;
    bool pinStateChanged : 1;
    bool lastPinState : 1;
    bool mustPresentation : 1;
};
static SystemState systemState;

#endif