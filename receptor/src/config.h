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
    constexpr long STATUS_INTERVAL = 5000;
    constexpr int MESSAGE_MAX_LEN = 128;
};
#endif