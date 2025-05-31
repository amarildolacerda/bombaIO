#include "Arduino.h"
#include "config.h"

#ifdef __AVR__
#include "RH_RF95.h"
#include <SoftwareSerial.h>
#include "LoRaRF95.h"
#endif

#ifdef ESP32
#ifdef HELTEC
#include <heltec.h>
#include <display_manager.h>
#endif
#include <LoRa32.h>
#endif

#include "logger.h"
#include "queue_message.h"

#ifdef WIFI
#include "WiFiManager.h"
WiFiManager wifiManager;

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#endif

#include <EEPROM.h>
#define EEPROM_ADDR_BOOT_COUNT 0
#define EEPROM_ADDR_LAST_BOOT 4
#define EEPROM_ADDR_PIN5_STATE 8
#define IDENTIFICATION_TIMEOUT_MS 10000UL
#define IDENTIFICATION_POWER_WINDOW_MS 60000UL

typedef void (*CallbackFunction)();
CallbackFunction callback;

void initCallback();

#ifdef WIFI
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = 0;

        DynamicJsonDocument doc(256);
        deserializeJson(doc, data);

        // Processa comando
        const char *cmd = doc["cmd"];
        if (strcmp(cmd, "display") == 0)
        {
            // const char *text = doc["text"];
            // Heltec.display->clear();
            // Heltec.display->drawString(0, 0, text);
            // Heltec.display->display();

            // Envia confirmação
            // JsonDocument resp;
            // resp["status"] = "ok";
            // resp["cmd"] = "display";
            // String output;
            // serializeJson(resp, output);
            // ws.textAll(output);
        }
    }
}
#endif

class App
{
private:
    long statusUpdater = 0;
    bool loraConnected = false;
    const char *terminalName = Config::TERMINAL_NAME;
    const uint8_t terminalId = Config::TERMINAL_ID;

    void initHardware()
    {
#ifdef HELTEC
        Heltec.begin(false, true, false, false, Config::LORA_BAND);
        // displayManager.init();
#endif
    }

public:
    void initLora()
    {
        loraConnected = lora.begin(terminalId, Config::LORA_BAND, Config::PROMISCUOS);
    }

    long sendUpdated = 0;

#ifdef WIFI
    void initWiFi()
    {
        // Configuração do WiFiManager
        wifiManager.setDebugOutput(true);
        wifiManager.setAPCallback([](WiFiManager *wm)
                                  { Logger::info("Modo AP iniciado. SSID: %s, IP: %s",
                                                 wm->getConfigPortalSSID().c_str(),
                                                 WiFi.softAPIP().toString().c_str()); });

        wifiManager.setSaveConfigCallback([]()
                                          { Logger::info("Configuração WiFi salva!"); });

        // Tenta conectar automaticamente
        if (!wifiManager.autoConnect(Config::TERMINAL_NAME))
        {
            Logger::error("Falha ao conectar e tempo limite atingido");
            ESP.restart();
        }

        Logger::info("Conectado ao WiFi! IP: %s", WiFi.localIP().toString().c_str());

        // Inicializa WebSocket e servidor web
        initWs();
    }
#endif

#ifdef WIFI
    void initWs()
    {
        // Configura WebSocket
        ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
                   {
        if (type == WS_EVT_CONNECT) {
            Logger::info("Cliente WebSocket #%u conectado", client->id());
        } else if (type == WS_EVT_DISCONNECT) {
            Logger::info("Cliente WebSocket #%u desconectado", client->id());
        } else if (type == WS_EVT_DATA) {
            handleWebSocketMessage(arg, data, len);
        } });

        server.addHandler(&ws);

        // Rota para página web
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Heltec WebSocket</title>
    <script>
        var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
        ws.onopen = function() {
            console.log('WebSocket conectado');
        };
        ws.onmessage = function(e) {
            console.log('Mensagem:', e.data);
            var logs = document.getElementById('logs');
            logs.value += e.data + '\n';
            logs.scrollTop = logs.scrollHeight;
        };
        ws.onerror = function(e) {
            console.error('Erro no WebSocket:', e);
        };
        function sendMessage() {
            var msg = document.getElementById('message').value;
            ws.send(msg);
            document.getElementById('message').value = '';
        }
    </script>
</head>
<body>
    <h1>Controle Heltec</h1>
    <textarea id="logs" rows="10" cols="50" readonly></textarea><br>
    <input type="text" id="message" placeholder="Digite um comando">
    <button onclick="sendMessage()">Enviar</button>
</body>
</html>
)rawliteral"); });

        // Inicia o servidor
        server.begin();
        Logger::info("Servidor Web iniciado na porta 80");

        // Configura o callback do Logger
        Logger::setLogCallback([](const String &message)
                               {
        if (ws.count()) {
            ws.textAll(message);
        } });
    }
#endif

    void setup()
    {
#ifdef HELTEC
        pinMode(Vext, OUTPUT);
        digitalWrite(Vext, LOW); // Ativar alimentação do LoRa
#endif
        Serial.begin(Config::SERIAL_SPEED);
        while (!Serial)
            ;

        initHardware();

        initCallback(); // antes de inciar os pacotes.

        initLora();
        if (!loraConnected)
        {
            Serial.println("LoRa failed to start");
        };

        pinMode(Config::LED_PIN, OUTPUT);
        pinMode(Config::RELAY_PIN, OUTPUT);
        initPinRelay();
        initWiFi();
        Serial.println("Pronto");
    }

    long ultimoReceived = 0;

    void loop()
    {
        if (!loraConnected)
            return;
        lora.loop();
        if (handleMessage())
        {
            ultimoReceived = millis();
        }

        if (millis() - sendUpdated > 100)
        {
            // sendUpdated = millis();

            // if (lora.connected)
            // {
            if (millis() - statusUpdater > Config::STATUS_INTERVAL)
            {
                setStatusChanged();
                statusUpdater = millis();
            }
            //  }

            if (millis() - ultimoReceived > 30000)
            {
                // initLora();
                ultimoReceived = millis();
                Logger::info("Tempo de atividade: %lu segundos", millis() / 1000);
#ifdef ESP32
                Logger::debug("Memória livre: %d bytes", ESP.getFreeHeap());
#endif
            }
        }
#ifdef WIFI
        ws.cleanupClients();
#endif
    }

    void sendEvent(uint8_t tid, const char *event, const char *value)
    {
        noInterrupts();
        lora.send(tid, event, value, terminalId);
        interrupts();
    }

    void savePinState(bool state)
    {
        noInterrupts();
#ifdef __AVR__
        EEPROM.update(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
#else
        EEPROM.write(EEPROM_ADDR_PIN5_STATE, state ? 1 : 0);
        EEPROM.commit();
#endif
        interrupts();
    }

    bool readPinState()
    {
        noInterrupts();
        uint8_t val = EEPROM.read(EEPROM_ADDR_PIN5_STATE);
        interrupts();
        return val == 1;
    }

    void initPinRelay()
    {
        noInterrupts();
        pinMode(Config::RELAY_PIN, OUTPUT);
        bool savedState = readPinState();
        digitalWrite(Config::RELAY_PIN, savedState ? HIGH : LOW);
        interrupts();

        Logger::info(savedState ? "Pin Relay initialized ON" : "Pin Relay initialized OFF");
    }

    bool handleMessage()
    {
        MessageRec rec;
        memset(&rec, 0, sizeof(MessageRec));

        bool hasMessage = lora.processIncoming(rec);

        if (!hasMessage)
        {
            return false;
        }
        Serial.print("handleMessage: {");
        Serial.print(rec.event);
        Serial.print("|");
        Serial.print(rec.value);
        Serial.println("}");

        bool handled = false;

        if (strstr(rec.event, "ack"))
            return true;
        if (strstr(rec.event, "nak"))
            return false;

        if (strstr(rec.event, "gpio"))
        {
            if (strstr(rec.value, "on"))
            {
                digitalWrite(Config::RELAY_PIN, HIGH);
                handled = true;
            }
            else if (strstr(rec.value, "off"))
            {
                digitalWrite(Config::RELAY_PIN, LOW);
                handled = true;
            }
            else if (strstr(rec.value, "toggle"))
            {
                int currentState = digitalRead(Config::RELAY_PIN);
                digitalWrite(Config::RELAY_PIN, !currentState);
                handled = true;
            }
            setStatusChanged();
        }

        return true;

        if (strstr(rec.event, "pub"))
        {

            lora.send(0, "pub", (char *)terminalName, terminalId);
            return true;
        }

        if (strstr(rec.event, "status"))
        {
            setStatusChanged();
            return true;
        }
        else if (strstr(rec.event, "reset"))
        {
            return true;
        }

        ack(rec.from, handled);
        return handled;
    }

    void ack(uint8_t tid, bool handled)
    {
        lora.send(tid, handled ? "ack" : "nak", "", terminalId);
    }

    void setStatusChanged()
    {
        lora.send(0, "status", digitalRead(Config::RELAY_PIN) ? "on" : "off", terminalId);
    }
};

App app;

void triggerCallback()
{
    CRITICAL_SECTION
    {
        app.savePinState(digitalRead(Config::RELAY_PIN));
        interrupts();
        app.setStatusChanged();
        systemState.pinStateChanged = true;
    }
}

static bool callbackInstaled = false;
void initCallback()
{
    if (callbackInstaled)
    {
        callbackInstaled = true;
        detachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN));
    }
    attachInterrupt(digitalPinToInterrupt(Config::RELAY_PIN), triggerCallback, CHANGE);
}