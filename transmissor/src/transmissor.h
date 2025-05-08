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

// ========== Configurações do Sistema ==========
namespace Config
{
    constexpr int TERMINAL_ID = 0x00;
    constexpr int MESSAGE_LEN = 128;
    // Hardware - Pinos LoRa (TTGO LoRa32 v1)
    constexpr uint8_t LORA_CS_PIN = 18;    // GPIO18 - Chip Select
    constexpr uint8_t LORA_RESET_PIN = 14; // GPIO14 - Reset
    constexpr uint8_t LORA_IRQ_PIN = 26;   // GPIO26 - Interrupção
    constexpr uint32_t LORA_BAND = 868E6;  // Banda para América do Sul
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
    constexpr uint32_t STATE_CHECK_INTERVAL = 3000;
    constexpr uint32_t PRESENTATION_INTERVAL = 5000;
    constexpr uint32_t COMMAND_TIMEOUT = 3000;

    // Tuya IoT
    constexpr const char *LPID = "sshilmfl"; // Substituir pelo PID real
    constexpr const char *LMCU_VER = "1.0.0";
}

// ========== Gerenciamento de Estado ==========
class SystemState
{
private:
    String relayState = "DESCONHECIDO";
    uint32_t lastUpdate = 0;
    bool wifiConnected = false;
    bool loraInitialized = false;
    uint32_t lastDisplayUpdate = 0;

protected:
    String loraRcvEvent = "";
    String loraRcvValue = "";

public:
    char loraRcvMessage[256] = ""; // Define um buffer fixo para armazenar mensagens recebidas
    void loraRcv(const String &message);
    void resetDisplayUpdate();
    void updateState(const String &newState);
    void setWifiStatus(bool connected);
    void setLoraStatus(bool initialized);
    void setLoraEvent(const String &event, const String &value);
    String getState() const;
    bool isWifiConnected() const;
    bool isLoraInitialized() const;
    bool isStateValid() const;
    String getISOTime();
    void updateDisplay();
    void conditionalUpdateDisplay();
};

extern SystemState systemState;

// ========== Comunicação LoRa ==========
namespace LoRaCom
{
    bool initialize();
    bool sendCommand(const String event, const String value, uint8_t tid = 0xFF);
    void processIncoming();
    bool waitAck();
    void sendPresentation(uint8_t tid = 0xFF, uint8_t n = 3);
    void ack(bool ak = true, uint8_t tid = 0xFF);
    void formatMessage(char *message, uint8_t tid, const char *event, const char *value);
    void sleep(unsigned int duration);
    uint8_t genHeaderId();
    void sendHeaderTo(uint8_t tid);

}

// ========== Callback Tuya ==========
unsigned char handleTuyaCommand(unsigned char dp_id, const unsigned char dp_data[], unsigned short dp_len);

// ========== Funções de Rede ==========
void initWiFi();
void initNTP();

// ========== Web Server ==========
void handleRootRequest();
void handleStateRequest();
void initWebServer();

// ========== Inicialização do Tuya ==========
void initTuya();

// ========== Setup e Loop Principais ==========
void setup();
void loop();

#endif // TRANSMISSOR_H