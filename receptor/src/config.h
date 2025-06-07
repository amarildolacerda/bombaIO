#ifndef CONFIG_H
#define CONFIG_H
#include "Arduino.h"

namespace Config
{
    constexpr const char *TIMEZONE = "BRT3BRST";
    constexpr const char *NTP_SERVER = "d.st1.ntp.br";
    constexpr const int WEBSERVER_PORT = 80;
    constexpr int MIN_RSSI_THRESHOLD = -120; // ajuste conforme necessário
    constexpr float MIN_SNR_THRESHOLD = -20.0;

    constexpr const char TERMINAL_NAME[] = "bemtevi"; // VTERMINAL_NAME;

    constexpr const bool PROMISCUOS = LORA_PROMISCUOS;
    constexpr const int TERMINAL_ID = VTERMINAL_ID;
    constexpr const int LED_PIN = LED_BUILTIN;

#if defined(TTGO) || defined(HELTEC)
    constexpr uint32_t LORA_BAND = 868E6; // usado esp32 TTGO LoRa32 v1
#elif RF95
    constexpr uint32_t LORA_BAND = 868.0; // usado RF95
#endif
#ifdef HELTEC
#define LORA_SF 7
#define LORA_BW 125E3
#define LORA_PW 20
#define LORA_CR 5
#define LORA_AUTO false
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
    bool lastPinState : 1;
    bool mustPresentation : 1;
    String startedISODate = "";
    String lastUpdateTime = "";

#ifdef ESP32
    String getISOTime(String frmt = "%Y-%m-%dT%H:%M:%SZ")
    {
#ifdef __AVR__
        return "1970-01-01T00:00:00Z"; // Fallback for AVR
#else

        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            return "1970-01-01T00:00:00Z";
        }

        char timeStr[25];
        strftime(timeStr, sizeof(timeStr), frmt.c_str(), &timeinfo);
        return String(timeStr);
#endif
    }
#endif
};
static SystemState systemState;

#endif