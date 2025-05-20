#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <map>
#include <pgmspace.h> // Use pgmspace.h for ESP8266
#elif ESP32
#include <WiFi.h>
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

#ifdef ESP32
#include <WebServer.h>
#include "Espalexa.h"
WebServer server(Config::WEBSERVER_PORT);
Espalexa espalexa;
#elif ESP8266
ESP8266WebServer server(Config::WEBSERVER_PORT);
#include "Espalexa.h"
Espalexa espalexa;
#endif

void processIncoming(LoRaInterface *loraInstance);

// ========== Instâncias Globais ==========

// ========== Espalexa (Alexa) Integration ==========

struct AlexaDeviceMap
{
    uint8_t tid;     // ID do dispositivo LoRa
    uint8_t alexaId; // ID atribuído pela Espalexa
    String name;     // Nome do dispositivo
};

static std::vector<AlexaDeviceMap> alexaDevices;

void alexaDeviceCallback(EspalexaDevice *d)
{
    const uint8_t alexaId = d->getId();
    for (auto &dev : alexaDevices)
    {
        if (dev.alexaId == alexaId)
        {
            const bool state = (d->getValue() > 0);
            Logger::info("Alexa(" + String(dev.alexaId) + "): " + dev.name + " command: " + String(state ? "ON" : "OFF"));
            LoRaCom::sendCommand("gpio", state ? "on" : "off", dev.tid);
            break;
        }
    }
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

void discoverableCallback(bool discoverable)
{
    espalexa.setDiscoverable(discoverable);
    Logger::info(String("Alexa Discoverable " + String(discoverable ? "OK" : "OFF")).c_str());
}

void aliveOffLineAlexa()
{

    for (auto &dev : alexaDevices)
    {
        int idx = DeviceInfo::dataOf(dev.tid);
        int secs = 60;
        if (idx >= 0)
        {
            DeviceInfoData &data = DeviceInfo::deviceList[idx];
            secs = DeviceInfo::getTimeDifferenceSeconds(data.lastSeenISOTime);
        }
        if (secs >= 60)
        {
            EspalexaDevice *d = espalexa.getDevice(dev.alexaId);
            if (d)
            {
                d->setState(false);
                d->setValue(false);
                d->setPercent(0);

                //                d->setPropertyChanged(EspalexaDeviceProperty::none);

                Logger::warn(String(dev.name + " esta a mais de " + String(secs) + "s sem conexao ").c_str());
                delay(10);
            }
        }
    }
}

void initAlexa()
{
    systemState.registerDiscoveryCallback(discoverableCallback);
    Logger::info("Alexa Init");
    uint8_t alexaId = 0;
    for (int i = 0; i < DeviceInfo::deviceRegList.size(); i++)
    {
        const DeviceRegData reg = DeviceInfo::deviceRegList[i];
        if (reg.tid == 0)
            continue;
        alexaDevices.push_back({reg.tid, alexaId++, reg.name});
        String name = reg.name + ":" + String(reg.tid);
        espalexa.addDevice(name, alexaDeviceCallback); //, EspalexaDeviceType::onoff);
    }

    server.onNotFound([]()
                      {
           String uri = server.uri();
           String arg0 = server.args() > 0 ? server.arg(0) : String("");
           if (!espalexa.handleAlexaApiCall(uri, arg0)) {
               server.send(404, "text/plain", "Not found");
               return;
           }
             server.send(200,"text/plain","OK"); });

    espalexa.begin(&server);
    for (auto dev : alexaDevices)
    {
        Logger::warn(String("Reg Alexa(" + String(dev.alexaId) + "): " + String(dev.tid) + " Name: " + dev.name).c_str());
    }
}

// ========== Main Setup and Loop ==========
#ifndef TEST
void tsetup()
{

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
    HtmlServer::initWebServer(&server);
    HtmlServer::begin();
    initAlexa();

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

#ifndef __AVR__
    HtmlServer::process();
#if defined(ESP32) || defined(ESP8266)
    espalexa.loop();
#endif

#endif

    systemState.handle();

#ifdef TTGO
    displayManager.handle();
#endif

    if (primeiraVez)
    {
        Logger::log(LogLevel::VERBOSE, F("Loop finalizado"));
        primeiraVez = false;
    }
}

void updateStateAlexa(uint8_t tid, String dname, String value)
{
    uint8_t ct = 0;
    for (auto &dev : alexaDevices)
    {
        if (dev.tid == tid)
        {
            ct++;
            EspalexaDevice *d = espalexa.getDevice(dev.alexaId);
            if (d)
            {
                d->setState(value == "on");
                d->setValue(value == "on");
                d->setPercent((value == "on") ? 100 : 0);
                char msg[64];
                sprintf(msg, "Term: % d Alexa %s (%d): %s (%s)", tid, dev.name, dev.alexaId, value.c_str(), value == "on" ? "true" : "false");
                Logger::info(msg);
                return;
            }
            else
            {
            }
        }
    }
    if (ct == 0)
    {
        Logger::error(String("nao achei alexa device" + String(tid)).c_str());
    }
}
void processIncoming(LoRaInterface *loraInstance)
{
    bool handled = false;
    uint8_t buf[Config::MESSAGE_LEN + 1]; // +1 para garantir espaço para null-terminator
    memset(buf, 0, sizeof(buf));
    uint8_t len = Config::MESSAGE_LEN;
    bool rt = loraInstance->receiveMessage(buf, len);
    if (!rt)
    {
        Logger::log(LogLevel::INFO, F("Não pegou ou descartou"));
        return;
    }

    uint8_t tid = loraInstance->headerFrom();
    if ((tid == 0) && (loraInstance->headerTo() == 0xFF)) // descarta pacotes proprios.
        return;

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
        // não é um dado do protocolo alto (ACK,NAK)
        return;
    }

    // Filtro: descarta mensagens que não começam com '{'
    if (buf[0] != '{')
    {
        LoRaCom::ack(false, loraInstance->headerFrom());
        Logger::log(LogLevel::ERROR, "Descartado pacote LoRa inválido (não começa com '{')");
        Logger::log(LogLevel::ERROR, (const char *)buf);
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
            DeviceInfoData data;
            data.tid = tid;
            data.event = event;
            data.value = value;
            data.name = dname;
            data.lastSeenISOTime = DeviceInfo::getISOTime();
            data.rssi = loraInstance->packetRssi();
            DeviceInfo::updateDeviceList(data.tid, data);

            LoRaCom::ack(true, loraInstance->headerFrom());
            systemState.updateState(value);

            if (!systemState.isDiscovering())
            {
                aliveOffLineAlexa();
                updateStateAlexa(tid, dname, value);
            }
            return;
        }
        else
        {
            Logger::error("não achei alexa data");
        }
        return;

        if (event == "presentation")
        {
            if (systemState.isDiscovering())
            {
                LoRaCom::ack(true, loraInstance->headerFrom());
                DeviceRegData reg;
                reg.tid = tid;
                reg.name = dname;
                DeviceInfo::updateRegList(tid, reg);
                Prefers::saveRegs();
                displayManager.message("Novo: " + dname);
                uint8_t alexaId = alexaDevices.size();
                alexaDevices.push_back({tid, alexaId, dname});
                espalexa.addDevice((reg.name + String(reg.tid)), alexaDeviceCallback, EspalexaDeviceType::onoff);
                systemState.setDiscovery(false);
                delay(10000);
                ESP.restart();
            }
            else
            {
                Logger::log(LogLevel::ERROR, F("Não esta em espera para novo dispositivo"));
            }
            return;
        }
    }
    LoRaCom::ack(false, loraInstance->headerFrom());
}

#endif
