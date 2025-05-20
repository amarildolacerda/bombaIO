#ifndef CONFIG_H
#define CONFIG_H

class SystemState
{
public:
    long previousMillis = 0;
    bool pinStateChanged : 1;
    bool lastPinState : 1;
    bool mustPresentation : 1;
};

static SystemState systemState;

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

    constexpr int BAND = 868.0; // Grove funciona melhor em 868

    constexpr int RELAY_PIN = 4;
    constexpr int RX_PIN = 6;
    constexpr int TX_PIN = 7;
    constexpr long STATUS_INTERVAL = 30000;
    constexpr long PRESENTATION_INTERVAL = 10000;
    constexpr int MESSAGE_MAX_LEN = 128;
};
#endif