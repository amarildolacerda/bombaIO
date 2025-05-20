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
#include "fauxmoESP.h"

#ifdef ESP32
// #include <WebServer.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(Config::WEBSERVER_PORT);
fauxmoESP alexa;

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
    String uniqueName()
    {
        return name + "." + String(tid);
    }
};

static std::vector<AlexaDeviceMap> alexaDevices;

void alexaDeviceCallback(unsigned char device_id, const char *device_name, bool state, unsigned char value)
{
    const uint8_t alexaId = (uint8_t)device_id;
    Logger::log(LogLevel::DEBUG, String("Callback da Alexa: " + String(device_name)).c_str());
    for (auto &dev : alexaDevices)
    {
        if (dev.uniqueName() == device_name)
        {
            // const bool state = (value > 0);
            Logger::info("Alexa(" + String(dev.alexaId) + "): " + dev.uniqueName() + " command: " + String(state ? "ON" : "OFF"));
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
        displayManager.message("Reiniciando: sem wifi");
        Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi");
        delay(10000);
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
    // espalexa.setDiscoverable(discoverable);
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
            alexa.setState(dev.uniqueName().c_str(), false, 0);

            // EspalexaDevice *d = espalexa.getDevice(dev.alexaId);
            // if (d)
            // {
            //     d->setState(false);
            //     d->setValue(false);
            //     d->setPercent(0);

            //                d->setPropertyChanged(EspalexaDeviceProperty::none);

            Logger::warn(String(dev.name + " esta a mais de " + String(secs) + "s sem conexao ").c_str());
            delay(10);
            //}
        }
    }
}

void initAlexa()
{
    systemState.registerDiscoveryCallback(discoverableCallback);
    Logger::info("Alexa Init");
    alexa.createServer(true);
    alexa.setPort(80);

    alexa.enable(true);

    uint8_t alexaId = 0;
    for (int i = 0; i < DeviceInfo::deviceRegList.size(); i++)
    {
        DeviceRegData reg = DeviceInfo::deviceRegList[i];
        if (reg.tid == 0)
            continue;
        alexaDevices.push_back({reg.tid, alexaId++, reg.name});

        alexa.addDevice(reg.uniqueName().c_str());
    }

    alexa.onSetState([](unsigned char device_id, const char *device_name, bool state, unsigned char value)
                     {
                         // Callback when a command from Alexa is received.
                         // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
                         // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
                         // Just remember not to delay too much here, this is a callback, exit as soon as possible.
                         // If you have to do something more involved here set a flag and process it in your main loop.

                         // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
                         // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
                         // if (2 == device_id) analogWrite(LED1_PIN, value);

                        // Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

                         // For the example we are turning the same LED on and off regardless fo the device triggered or the value
                        // digitalWrite(LED, !state); // we are nor-ing the state because our LED has inverse logic.

                        alexaDeviceCallback(device_id,device_name,state,value); });

    // These two callbacks are required for gen1 and gen3 compatibility
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
                             Serial.print("onRequestBody");
                             Serial.println(request->url());
                             if (alexa.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data)))
                                 return;
                             // Handle any other body request here...
                         });
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
                          Serial.print("onNotFound");
                          Serial.println(request->url());
                          String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
                          if (alexa.process(request->client(), request->method() == HTTP_GET, request->url(), body))
                              return;
                          // Handle not found request here...
                      });

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
    // espalexa.loop();
    alexa.handle();
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

void updateStateAlexa(uint8_t tid, String uniqueName, String value)
{
    uint8_t ct = 0;
    Serial.print(uniqueName);
    for (auto &dev : alexaDevices)
    {
        if (dev.uniqueName() == uniqueName)
        {
            ct++;
            // EspalexaDevice *d = espalexa.getDevice(dev.alexaId);
            // if (d)
            //{
            //     d->setState(value == "on");
            //     d->setValue(value == "on");
            //     d->setPercent((value == "on") ? 100 : 0);
            alexa.setState(dev.uniqueName().c_str(), value == "on", value == "on");
            char msg[64];
            sprintf(msg, "Term: % d Alexa %s (%d): %s (%s)", tid, dev.uniqueName(), dev.alexaId, value.c_str(), value == "on" ? "true" : "false");
            Logger::info(msg);
            return;
            //}
            //}
        }
        if (ct == 0)
        {
            Logger::error(String("nao achei alexa device" + String(tid)).c_str());
        }
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
                updateStateAlexa(tid, data.uniqueName(), value);
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
                alexa.addDevice(reg.uniqueName().c_str());
                // espalexa.addDevice((reg.name + ":" + String(reg.tid)), alexaDeviceCallback, EspalexaDeviceType::onoff);
                systemState.setDiscovery(false);
                // delay(10000);
                //  ESP.restart();
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
