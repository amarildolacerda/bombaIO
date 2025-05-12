#ifndef CONFIG_H
#define CONFIG_H

namespace Config
{
#ifdef __AVR__
    constexpr char TERMINAL_NAME[] = "relay";
#else
    constexpr char TERMINAL_NAME[] = RELAY_LORA_NAME;
#endif

    constexpr int BAND = 868.0; // Grove funciona melhor em 868
    constexpr bool PROMISCUOS = true;
    constexpr int TERMINAL_ID = LORA_TERMINAL_ID;
}

#endif