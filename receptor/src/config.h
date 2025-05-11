#ifndef CONFIG_H
#define CONFIG_H

namespace Config
{
    constexpr char TERMINAL_NAME[] = "relay";

    constexpr int BAND = 868.0; // Grove funciona melhor em 868
    constexpr bool PROMISCUOS = true;
    constexpr int TERMINAL_ID = 0x01;
}

#endif