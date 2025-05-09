#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#endif

#include "transmissor.h"
#include "logger.h"
#include <map>
#include "config.h"
#include "html_server.h"
#include "LoRaCom.h"
#include "device_info.h"
#include "system_state.h"
#include "display_manager.h"

// ========== Instâncias Globais ==========
TuyaWifi my_device;

// ========== Implementações ==========

// ========== Tuya Callback Implementation ==========
unsigned char handleTuyaCommand(unsigned char dp_id, const unsigned char dp_data[], unsigned short dp_len)
{
    switch (dp_data[0])
    {
    case 0:
        LoRaCom::sendCommand("desligar", "OFF", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: DESLIGAR");
        break;
    case 1:
        LoRaCom::sendCommand("ligar", "OK", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: LIGAR");
        break;
    case 2:
        LoRaCom::sendCommand("revert", "REVERTER", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: REVERTER");
        break;
    case 3:
        LoRaCom::sendCommand("status", "", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: STATUS");
        break;
    default:
        //  Logger::log(LogLevel::WARNING, "Comando Tuya desconhecido: " + String(dp_data[0]));
        return 0;
    }
    systemState.resetDisplayUpdate();
    return 1;
}

// ========== Network Functions Implementations ==========
void initWiFi()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initWiFi");
    WiFiManager wifiManager;
    wifiManager.setTimeout(Config::WIFI_TIMEOUT_S);

    if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
    {
        Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi");
        ESP.restart();
    }

    systemState.setWifiStatus(true);
    // Logger::log(LogLevel::INFO, "WiFi conectado: " + WiFi.localIP().toString());
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWiFi com sucesso");
}

void initNTP()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initNTP");
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    Logger::log(LogLevel::INFO, "Sincronizando com NTP...");
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        Logger::log(LogLevel::WARNING, "Falha ao sincronizar com NTP. Tentando novamente em 1 minuto...");
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com falha");
    }
    else
    {
        Logger::log(LogLevel::INFO, "Sincronização com NTP bem-sucedida.");
        // Logger::log(LogLevel::VERBOSE, "Horário sincronizado: " + String(asctime(&timeinfo)));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com sucesso");
    }
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

#ifdef ESP8266
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
        ESP.restart();
    }

    initTuya();
    HtmlServer::initWebServer();

#ifdef ESP8266
    Logger::log(LogLevel::INFO, "ESP8266 não possui suporte para DisplayManager. Ignorando inicialização do DisplayManager.");
#else
    DisplayManager::updateDisplay();
#endif

    LoRa.receive();
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: setup");
}

static uint32_t lastStateCheck = 0;
static uint32_t lastPresentationTime = 0; // Adiciona um controle para o envio de Presentation

void loop()
{
    //  LoRa.idle();
    // LoRaCom::processIncoming();
    my_device.uart_service();
    HtmlServer::process();

    systemState.conditionalUpdateDisplay();

    // Verifica o estado a cada intervalo definido
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

    // Envia mensagem de Presentation a cada 1 minuto
    if (millis() - lastPresentationTime > Config::PRESENTATION_INTERVAL) // 60000 ms = 1 minuto
    {
        LoRaCom::sendPresentation(0xFF, 1);
        lastPresentationTime = millis();
    }
    //   LoRa.receive();
}