
#ifndef WSLOGGER_H
#define WSLOGGER_H

#include "Arduino.h"
#include "logger.h"
#include "config.h"
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>
AsyncWebSocket ws("/ws");

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

void logCallbackFn(const String &message)
{
    if (ws.count())
    {
        ws.textAll(message);
    }
}

namespace WSLogger
{
    AsyncWebServer *wserver = nullptr;
#ifdef WIFI
    static void initWs(AsyncWebServer &server)
    {
        wserver = &server;
        Logger::setLogCallback(logCallbackFn);

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

        wserver->addHandler(&ws);
        // Adicione esta rota para testar o WebSocket
        wserver->on("/test", HTTP_GET, [](AsyncWebServerRequest *request)
                    { request->send(200, "text/plain", "Servidor Web ativo"); });

        // Rota para página web
        wserver->on("/ws", HTTP_GET, [](AsyncWebServerRequest *request)
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
    }
#endif

}

#endif