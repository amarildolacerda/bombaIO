#ifndef CONFIG_H
#define CONFIG_H

class SystemState
{
public:
    unsigned long previousMillis = 0;
    bool pinStateChanged = false;
    int lastPinState = HIGH;
    bool mustPresentation = true;
};

static SystemState systemState;

namespace Config
{
#ifdef __AVR__
    constexpr char TERMINAL_NAME[] = "relay";
#else
    constexpr char TERMINAL_NAME[] = "D1";
#endif

    constexpr bool PROMISCUOS = LORA_PROMISCUOS;
    constexpr int TERMINAL_ID = LORA_TERMINAL_ID;
    constexpr int LED_PIN = LED_BUILTIN;

    constexpr int BAND = 868.0; // Grove funciona melhor em 868

    constexpr int RELAY_PIN = 4;
    constexpr long STATUS_INTERVAL = 30000;
    constexpr long PRESENTATION_INTERVAL = 10000;
};
#endif