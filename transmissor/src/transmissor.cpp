#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <map>
#include <pgmspace.h> // Use pgmspace.h for ESP8266
#elif ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <map>
// #include <pgmspace> // Use pgmspace for ESP32
#endif

#include "transmissor.h"
#include "logger.h"
#include "config.h"
#include "html_server.h"
#include "LoRaCom.h"
#include "device_info.h"
#include "system_state.h"
#ifdef TTGO
#include "display_manager.h"
#endif
#include "LoRaInterface.h"

void processIncoming(LoRaInterface *loraInstance);

// ========== Instâncias Globais ==========
#ifdef TUYA
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
    /*  static const char *const logs[] PROGMEM = {
          logTuyaDesligar,
          logTuyaLigar,
          logTuyaReverter,
          logTuyaStatus};
  */
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
    return 1;
}
#endif

// ========== Network Functions Implementations ==========
#if defined(ESP8266) || defined(ESP32)
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
    char ipBuffer[16]; // Buffer to store IP address as a string
    snprintf(ipBuffer, sizeof(ipBuffer), "%s", WiFi.localIP().toString().c_str());
    // Logger::log(LogLevel::INFO, FPSTR(logWiFiConnected));
    Logger::log(LogLevel::INFO, ipBuffer);
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWiFi com sucesso");
}
#endif

#if defined(ESP8266) || defined(ESP32)
void initNTP()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initNTP");
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    Logger::log(LogLevel::INFO, "Sincronizando com NTP...");
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        // Logger::log(LogLevel::WARNING, FPSTR(logNTPFail));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com falha");
    }
    else
    {
        // Logger::log(LogLevel::INFO, FPSTR(logNTPSuccess));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com sucesso");
    }
}
#endif

#ifdef TUYA
// ========== Tuya Initialization ==========
void initTuya()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initTuya");
    my_device.init((unsigned char *)Config::LPID, (unsigned char *)Config::LMCU_VER);
    my_device.dp_process_func_register(handleTuyaCommand);
    Logger::log(LogLevel::INFO, "Tuya inicializado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initTuya");
}
#endif

// ========== Main Setup and Loop ==========
#ifndef TEST
void setup()
{
#ifdef DEBUG_ON
    Logger::setLogLevel(LogLevel::VERBOSE);
#endif
    Logger::log(LogLevel::VERBOSE, F("Entrando no procedimento: setup"));
    Serial.begin(Config::SERIAL_BAUD);
    Logger::log(LogLevel::INFO, F("Iniciando sistema..."));

#if defined(ESP8266) || defined(__AVR__)
    Logger::log(LogLevel::WARNING, F("ESP8266 não possui display."));
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

#if defined(ESP32) || defined(ESP8266)
    initWiFi();
    initNTP();
#endif

    if (!LoRaCom::initialize())
    {
        Logger::log(LogLevel::ERROR, F("Falha crítica no LoRa - reiniciando"));
        // ESP.restart();
    }
    LoRaCom::setReceiveCallback(processIncoming);

#ifdef TUYA
    initTuya();
#endif
#if defined(ESP32) || defined(ESP8266)
    HtmlServer::initWebServer();
#endif

#ifdef ESP8266
    Logger::log(LogLevel::INFO, "ESP8266 não possui suporte para DisplayManager. Ignorando inicialização do DisplayManager.");
#elif TTGO
    displayManager.updateDisplay();
#endif

#ifdef DEBUG_ON
    Logger::log(LogLevel::VERBOSE, F("Saindo do procedimento: setup"));
#endif
}

#endif

#ifndef TEST
static bool primeiraVez = true;
void loop()
{
    if (primeiraVez)
    {
        Logger::log(LogLevel::VERBOSE, F("Loop iniciado"));
    }

    LoRaCom::handle(); // precisa pedir leitura rapida
#ifdef TUYA
    my_device.uart_service();
#endif

#ifndef __AVR__
    HtmlServer::process();

#endif

    systemState.handle();

    if (!systemState.isStateValid())
    {
        LoRaCom::sendCommand("get", "status", 0xFF);
        systemState.resetStateValid();
#ifdef TTGO
        displayManager.message("get status");
#endif
    }

#ifdef TTGO
    displayManager.handle();
#endif

    if (primeiraVez)
    {
        Logger::log(LogLevel::VERBOSE, F("Loop finalizado"));
        primeiraVez = false;
    }
}

void processIncoming(LoRaInterface *loraInstance)
{
    // while (loraInstance->available())
    // {
    uint8_t buf[Config::MESSAGE_LEN + 1] = {0}; // +1 para garantir espaço para null-terminator
    uint8_t len = Config::MESSAGE_LEN;
    bool rt = loraInstance->receiveMessage(buf, len);
    if (!rt)
    {
        Logger::log(LogLevel::INFO, F("Não pegou ou descartou"));
        return;
    }
    buf[len] = '\0'; // Garante null-terminator para uso como string

    // Remove caracteres não imprimíveis antes de desserializar
    for (uint8_t i = 0; i < len; i++)
    {
        if (buf[i] < 32 || buf[i] > 126)
        {
            buf[i] = ' '; // Substitui caracteres inválidos por espaço
        }
    }

    if (len <= 10)
    {
        // não é um dado do protocolo alto
        return;
    }

    // Substituir DynamicJsonDocument por StaticJsonDocument para evitar alocação dinâmica
    StaticJsonDocument<128> doc;                                          // Reduzido para 256 bytes
    DeserializationError error = deserializeJson(doc, (const char *)buf); // Removido o uso de `len`
    if (error)
    {
        Logger::log(LogLevel::ERROR, F("Falha ao converter buf em JSON"));
        Logger::log(LogLevel::ERROR, (const char *)buf);
        Logger::log(LogLevel::ERROR, error.c_str()); // Log detalhado do erro
        return;
    }
    static String event;
    if (doc.containsKey("event"))
        event = doc["event"].as<String>();
    static String value = "";
    if (doc.containsKey("value"))
        value = doc["value"].as<String>();
    // Agora 'doc' contém o JSON parseado de 'buf'
    if (event)
    {
#ifdef TTGO
        displayManager.setEvent(loraInstance->headerFrom(), event, value);
#endif
        if (event == "status")
        {
            systemState.updateState(value);
        }
        DeviceInfoData data;
        data.tid = loraInstance->headerFrom();
        data.event = event;
        data.value = value;
        data.name = doc["dtype"].as<String>();
        data.lastSeenISOTime = DeviceInfo::getISOTime();
        data.rssi = loraInstance->packetRssi();
        DeviceInfo::updateDeviceList(data.tid, data);
    }
    Serial.println((char *)buf);
    LoRaCom::ack(true, loraInstance->headerFrom());
    // }
}

#endif
