
#ifndef CONFIG_H
#define CONFIG_H

// Compiler: The project is compiled using the ESP32 toolchain.
// Platform: The project is designed to run on the ESP32 platform.
// Libraries: The project uses the ESP32 Arduino core libraries and other related libraries for LoRa, OLED display, and Wi-Fi functionalities.
#include <Arduino.h>

#ifdef __AVR__
#define Dx 10
#define Dt 11
#endif
// ========== Configurações do Sistema ==========
namespace Config
{
#if defined(RF95) || defined(LORASERIAL) || defined(ESP8266)

    constexpr uint8_t LORA_RX_PIN = Dx; // RX pin for RF95
    constexpr uint8_t LORA_TX_PIN = Dt; // TX pin for RF95
#endif
    constexpr int TERMINAL_ID = 0x00;
    constexpr int MESSAGE_LEN = 128;
    // Hardware - Pinos LoRa (TTGO LoRa32 v1)
    constexpr uint8_t LORA_CS_PIN = 18;    // GPIO18 - Chip Select
    constexpr uint8_t LORA_RESET_PIN = 14; // GPIO14 - Reset
    constexpr uint8_t LORA_IRQ_PIN = 26;   // GPIO26 - Interrupção
#ifdef TTGO
    constexpr uint32_t LORA_BAND = 868E6; // usado esp32 TTGO LoRa32 v1
#elif RF95
    constexpr uint32_t LORA_BAND = 868.0; // usado RF95
#endif
    constexpr uint16_t LORA_SYNC_WORD = 0xF3;

    constexpr int MIN_RSSI_THRESHOLD = -120; // ajuste conforme necessário
    constexpr float MIN_SNR_THRESHOLD = -20.0;
    constexpr int ACK_TIMEOUT_MS = 100;

    // Display OLED
    constexpr uint8_t SCREEN_WIDTH = 128;
    constexpr uint8_t SCREEN_HEIGHT = 64;
    constexpr uint8_t OLED_ADDRESS = 0x3C;
    constexpr uint32_t DISPLAY_UPDATE_INTERVAL = 1000; // Atualização a cada 1s

    // Rede
    constexpr uint16_t WEBSERVER_PORT = 80;
    constexpr uint16_t WIFI_TIMEOUT_S = 180;
    constexpr const char *WIFI_AP_NAME = "TTGO_Transmitter";
    constexpr const char *NTP_SERVER = "pool.ntp.org";
    constexpr long GMT_OFFSET_SEC = -10800; // UTC-3 (Brasília)
    constexpr int DAYLIGHT_OFFSET_SEC = 0;

    // Sistema
    constexpr uint32_t SERIAL_BAUD = 115200;
    constexpr uint32_t STATE_TIMEOUT_MS = 300000 / 5; // 5 minutos (status automático)
    constexpr uint32_t STATE_CHECK_INTERVAL = 5000;
    constexpr uint32_t PRESENTATION_INTERVAL = 15000;
    constexpr uint32_t COMMAND_TIMEOUT = 3000;

    // Tuya IoT
    constexpr const char *LPID = "sshilmfl"; // Substituir pelo PID real
    constexpr const char *LMCU_VER = "1.0.0";
}
#endif