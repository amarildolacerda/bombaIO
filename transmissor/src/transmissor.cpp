#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <map>
#include <pgmspace.h> // Use pgmspace.h for ESP8266
#elif ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <map>
#include <pgmspace> // Use pgmspace for ESP32
#endif

#include "transmissor.h"
#include "logger.h"
#include "config.h"
#include "html_server.h"
#include "LoRaCom.h"
#include "device_info.h"
#include "system_state.h"
#include "display_manager.h"

// ========== Instâncias Globais ==========
TuyaWifi my_device;

// ========== PROGMEM Strings ==========
const char logTuyaDesligar[] PROGMEM = "Comando Tuya: DESLIGAR";
const char logTuyaLigar[] PROGMEM = "Comando Tuya: LIGAR";
const char logTuyaReverter[] PROGMEM = "Comando Tuya: REVERTER";
const char logTuyaStatus[] PROGMEM = "Comando Tuya: STATUS";
const char logWiFiConnected[] PROGMEM = "WiFi conectado: ";
const char logNTPFail[] PROGMEM = "Falha ao sincronizar com NTP. Tentando novamente em 1 minuto...";
const char logNTPSuccess[] PROGMEM = "Sincronização com NTP bem-sucedida.";

// ========== Implementações ==========

// ========== Tuya Callback Implementation ==========
unsigned char handleTuyaCommand(unsigned char dp_id, const unsigned char dp_data[], unsigned short dp_len)
{
    static const char *const commands[] PROGMEM = {"desligar", "ligar", "revert", "status"};
    static const char *const responses[] PROGMEM = {"OFF", "OK", "REVERTER", ""};
    static const char *const logs[] PROGMEM = {
        logTuyaDesligar,
        logTuyaLigar,
        logTuyaReverter,
        logTuyaStatus};

    if (dp_data[0] < 4)
    {
        char command[10];
        char response[10];
        strcpy_P(command, (PGM_P)pgm_read_ptr(&commands[dp_data[0]]));
        strcpy_P(response, (PGM_P)pgm_read_ptr(&responses[dp_data[0]]));
        LoRaCom::sendCommand(command, response, dp_id);
        Logger::log(LogLevel::INFO, command); // Or use the appropriate variable if you want to log the command string
    }
    else
    {
        return 0;
    }
    systemState.resetDisplayUpdate();
    return 1;
}

// ========== Network Functions Implementations ==========
void initWiFi()
{
#if defined(ESP8266) || defined(ESP32)
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initWiFi");
    WiFiManager wifiManager;
    wifiManager.setTimeout(Config::WIFI_TIMEOUT_S);

    if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
    {
        Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi");
        ESP.restart();
    }

    systemState.setWifiStatus(true);
    char ipBuffer[16]; // Buffer to store IP address as a string
    snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
    Logger::log(LogLevel::INFO, FPSTR(logWiFiConnected));
    Logger::log(LogLevel::INFO, ipBuffer);
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWiFi com sucesso");
#else
    Logger::log(LogLevel::WARNING, "initWiFi não suportado neste dispositivo");
#endif
}

void initNTP()
{
#if defined(ESP8266) || defined(ESP32)
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initNTP");
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    Logger::log(LogLevel::INFO, "Sincronizando com NTP...");
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        Logger::log(LogLevel::WARNING, FPSTR(logNTPFail));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com falha");
    }
    else
    {
        Logger::log(LogLevel::INFO, FPSTR(logNTPSuccess));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com sucesso");
    }
#else
    Logger::log(LogLevel::WARNING, "initNTP não suportado neste dispositivo");
#endif
}

// ========== Tuya Initialization ==========
void initTuya()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initTuya");
    my_device.init((unsigned char *)Config::LPID, (unsigned char *)Config::LMCU_VER);
    my_device.dp_process_func_register(handleTuyaCommand);
    Logger::log(LogLevel::INFO, "Tuya inicializado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initTuya");
}

// ========== Main Setup and Loop ==========
void setup()
{
    Logger::setLogLevel(LogLevel::VERBOSE);
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: setup");
    Serial.begin(Config::SERIAL_BAUD);
    Logger::log(LogLevel::INFO, "Iniciando sistema...");

#if defined(ESP8266) || defined(__AVR__)
    Logger::log(LogLevel::INFO, "ESP8266 não possui display. Ignorando inicialização do display.");
#else
    if (display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS))
    {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Iniciando...");
        display.display();
    }
    else
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar display OLED");
    }
#endif

    initWiFi();
    initNTP();

    if (!LoRaCom::initialize())
    {
        Logger::log(LogLevel::ERROR, "Falha crítica no LoRa - reiniciando");
        // ESP.restart();
    }

    initTuya();
#if defined(ESP32) || defined(ESP8266)
    HtmlServer::initWebServer();
#endif

#ifdef ESP8266
    Logger::log(LogLevel::INFO, "ESP8266 não possui suporte para DisplayManager. Ignorando inicialização do DisplayManager.");
#elif ESP32
    DisplayManager::updateDisplay();
#endif

    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: setup");
}

void loop()
{
    LoRaCom::handle();
    my_device.uart_service();
#ifndef __AVR__
    HtmlServer::process();
#endif
    systemState.conditionalUpdateDisplay();

    // Verifica o estado a cada intervalo definido
    static uint32_t lastStateCheck = 0;

    if (millis() - lastStateCheck > Config::STATE_CHECK_INTERVAL)
    {
        if (!systemState.isStateValid())
        {
            LoRaCom::sendCommand("get", "status", 0xFF);
            DisplayManager::eventEnviado("get status");
            systemState.resetDisplayUpdate();
        }
        lastStateCheck = millis();
    }
}