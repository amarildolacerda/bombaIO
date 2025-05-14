#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <map>
#include <pgmspace.h> // Use pgmspace.h for ESP8266
#elif ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <map>
#include "prefers.h"
#endif

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Espalexa.h>

#include "transmissor.h"
#include "logger.h"
#include "config.h"
#include "html_tserver.h"
#include "LoRaCom.h"
#include "device_info.h"
#include "system_state.h"
#ifdef TTGO
#include "display_manager.h"
#endif
#include "LoRaInterface.h"

void processIncoming(LoRaInterface *loraInstance);

// ========== Instâncias Globais ==========

// ========== Espalexa (Alexa) Integration ==========
Espalexa *espalexa = nullptr;

void alexaDeviceCallback(EspalexaDevice *d)
{
    const uint8_t item = d->getId();
    if ((item < 0) || (item >= DeviceInfo::deviceRegList.size()))
    {
        return;
    }
    const DeviceRegData data = DeviceInfo::deviceRegList[item].second;

    const bool state = (d->getValue() > 0);
    Logger::log(LogLevel::INFO, state ? "Alexa: ON" : "Alexa: OFF");
    LoRaCom::sendCommand("gpio", state ? "on" : "off", data.tid);
}

// ========== Network Functions Implementations ==========
#if defined(ESP8266) || defined(ESP32)
void initWiFi()
{
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
    Logger::log(LogLevel::INFO, ipBuffer);
}
#endif

#if defined(ESP8266) || defined(ESP32)
void initNTP()
{
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    Logger::log(LogLevel::INFO, "Sincronizando com NTP...");
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        return;
    }
}
#endif

void initAlexa()
{
    Serial.println("Alexa:");
    for (int i = 0; i < DeviceInfo::deviceRegList.size(); i++)
    {
        const DeviceRegData reg = DeviceInfo::deviceRegList[i].second;
        if (reg.tid == 0)
            continue;
        espalexa->addDevice((reg.name + String(reg.tid)), alexaDeviceCallback, EspalexaDeviceType::onoff);
        Serial.print(reg.tid);
        Serial.print(": ");
        Serial.println(reg.name + String(reg.tid));
    }
}

// ========== Main Setup and Loop ==========
#ifndef TEST
void tsetup(Espalexa *alexa)
{
    espalexa = alexa;

#ifdef DEBUG_ON
    Logger::setLogLevel(LogLevel::VERBOSE);
#endif
    Serial.begin(Config::SERIAL_BAUD);
    while (!Serial)
        ;
    Logger::log(LogLevel::INFO, F("Iniciando sistema..."));
    Prefers::restoreRegs();

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
    }
    LoRaCom::setReceiveCallback(processIncoming);

#if defined(ESP32) || defined(ESP8266)
    HtmlServer::initWebServer(alexa);
    initAlexa();
    HtmlServer::begin();

#endif

#ifdef ESP8266
    Logger::log(LogLevel::INFO, "ESP8266 não possui suporte para DisplayManager. Ignorando inicialização do DisplayManager.");
#elif TTGO
    displayManager.updateDisplay();
#endif
}

#endif

#ifndef TEST
static bool primeiraVez = true;
void tloop()
{
    if (primeiraVez)
    {
        Logger::log(LogLevel::VERBOSE, F("Loop iniciado"));
    }

    LoRaCom::handle(); // precisa pedir leitura rapida
#if defined(ESP32) || defined(ESP8266)
    espalexa->loop();
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

    static String dname = "UNKNOW";
    if (doc.containsKey("dtype"))
    {
        dname = doc["dtype"].as<String>();
    }
    static uint8_t tid = loraInstance->headerFrom();
    if ((tid == 0) || (tid == 0xFF))
        return;

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

        if (event == "presentation")
        {
            Serial.print("saveRegs");
            DeviceRegData reg;
            reg.tid = tid;
            reg.name = dname;
            DeviceInfo::updateRegList(tid, reg);
            Prefers::saveRegs();
        }

        DeviceInfoData data;
        data.tid = tid;
        data.event = event;
        data.value = value;
        data.name = dname;
        data.lastSeenISOTime = DeviceInfo::getISOTime();
        data.rssi = loraInstance->packetRssi();
        DeviceInfo::updateDeviceList(data.tid, data);
    }
    LoRaCom::ack(true, loraInstance->headerFrom());
    // }
}

#endif
