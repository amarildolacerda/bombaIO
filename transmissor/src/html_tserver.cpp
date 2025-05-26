#ifdef WS
#include "html_tserver.h"
#include "logger.h"
#include <Arduino.h>
#include "config.h"
#include "system_state.h"
#include "device_info.h"
#include "LoRaCom.h"
#include "prefers.h"
#include "display_manager.h"
#ifndef __AVR__

#ifdef ESP32
#include <Update.h>
#endif

namespace HtmlServer
{
    AsyncWebServer *espServer = nullptr;

    // Namespace para organizar todas as strings PROGMEM
    namespace ProgmemStrings
    {
        // Styles
        const char commonStyles[] PROGMEM = R"=====(
            * { box-sizing: border-box; }
            body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }
            .container { max-width: 800px; margin: 0 auto; }
            h1 { text-align: center; color: #2c3e50; margin-bottom: 20px; }
            .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 25px; margin-bottom: 25px; }
            h2 { color: #3498db; margin-top: 0; border-bottom: 1px solid #eee; padding-bottom: 10px; }
            .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 15px; }
            button, .btn { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; transition: all 0.3s; flex: 1 1 120px; text-decoration: none; display: inline-block; text-align: center; }
            button:hover, .btn:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
            button:active, .btn:active { transform: translateY(0); }
            .status { color: #7f8c8d; font-size: 14px; margin-top: 5px; }
            .btn-danger { background: #e74c3c; }
            .btn-danger:hover { background: #c0392b; }
            .btn-success { background: #2ecc71; }
            .btn-success:hover { background: #27ae60; }
            .btn-warning { background: #f39c12; }
            .btn-warning:hover { background: #d35400; }
            .btn-info { background: #3498db; }
            .btn-info:hover { background: #2980b9; }
            @media (max-width: 600px) { .button-group { flex-direction: column; } button, .btn { width: 100%; } }
            .menu { background: #2c3e50; padding: 15px; margin-bottom: 20px; border-radius: 8px; display: flex; flex-wrap: wrap; gap: 10px; }
            .menu-item { color: white; text-decoration: none; padding: 8px 12px; border-radius: 4px; }
            .menu-item:hover { background: #3498db; }
            .upload-form { margin-top: 20px; }
            .progress { margin-top: 20px; width: 100%; background-color: #f1f1f1; border-radius: 4px; overflow: hidden; }
            .progress-bar { width: 0%; height: 30px; background-color: #4CAF50; text-align: center; line-height: 30px; color: white; transition: width 0.3s; }
            .device-card { display: inline-block; width: 200px; margin: 10px; padding: 15px; border: 1px solid #ddd; border-radius: 8px; text-align: center; background-color: #fff; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }
            .device-card h3 { margin: 10px 0; font-size: 18px; color: #333; }
            .device-card button { margin-top: 10px; padding: 10px; background-color: #3498db; color: white; border: none; border-radius: 5px; cursor: pointer; }
            .device-card button:hover { background-color: #2980b9; }
        )=====";

        // Templates HTML
        const char menuHtml[] PROGMEM = R"=====(
            <div class='menu'>
                <a href='/' class='menu-item'>Home</a>
                <a href='/ota' class='menu-item'>OTA Update</a>
                <a href='/restart' class='menu-item'>Reiniciar</a>
                <a href='/discovery' class='menu-item' id='discovery-btn'>Novo</a>
            </div>
            <script>
                let discoveryActive = false;
                let discoveryTimeout;
                function toggleDiscovery() {
                    discoveryActive = !discoveryActive;
                    const btn = document.getElementById('discovery-btn');
                    if (discoveryActive) {
                        btn.style.backgroundColor = '#2ecc71';
                        fetch('/discovery?enable=1', {method: 'POST'});
                        discoveryTimeout = setTimeout(() => {
                            discoveryActive = false;
                            btn.style.backgroundColor = '';
                        }, 60000);
                    } else {
                        clearTimeout(discoveryTimeout);
                        btn.style.backgroundColor = '';
                        fetch('/discovery?enable=0', {method: 'POST'});
                    }
                }
                document.getElementById('discovery-btn').onclick = function(e) {
                    e.preventDefault();
                    toggleDiscovery();
                    return false;
                };
            </script>
        )=====";

        const char htmlHeader[] PROGMEM = R"=====(
            <!DOCTYPE html><html lang='pt-BR'>
            <head>
                <meta charset='UTF-8'>
                <meta name='viewport' content='width=device-width, initial-scale=1.0'>
                <title>{TITLE}</title>
                <style>
        )=====";

        const char htmlFooter[] PROGMEM = R"=====(
                </style>
            </head>
            <body>
                <div class='container'>
                    {MENU}
                    {CONTENT}
                </div>
                {SCRIPTS}
            </body>
            </html>
        )=====";

        const char deviceCardTemplate[] PROGMEM = R"=====(
            <div class='device-card'>
                <h3><div style='cursor:pointer;' onclick="window.location='/device?tid={TID}'">{NAME}</div></h3>
                <hr/><p id='device-status-{TID}' style="cursor:pointer;" onclick="fetch('/ctlDevice?tid={TID}&action=toggle', {method: 'POST'}).then(() => updateStatus({TID}));">...</p>
            </div>
        )=====";

        // Strings de texto
        const char offlineStatus[] PROGMEM = "OffLine";
        const char unknownStatus[] PROGMEM = "???";
        const char deviceNotFound[] PROGMEM = "Dispositivo não encontrado";
        const char updateSuccess[] PROGMEM = "Atualização concluída com sucesso! O dispositivo irá reiniciar.";
        const char updateError[] PROGMEM = "Erro na atualização: ";
    }

    // Função auxiliar para acessar strings PROGMEM
    String getProgmemString(const char *str)
    {
        String result;
        char buffer[strlen_P(str) + 1];
        strcpy_P(buffer, str);
        result = buffer;
        return result;
    }

    String getCommonStyles()
    {
        return getProgmemString(ProgmemStrings::commonStyles);
    }

    String generateMenu()
    {
        return getProgmemString(ProgmemStrings::menuHtml);
    }

    String generateDeviceCard(uint8_t tid, const String &name)
    {
        String card = getProgmemString(ProgmemStrings::deviceCardTemplate);
        card.replace("{TID}", String(tid));
        card.replace("{NAME}", name);
        return card;
    }

    String generateHtmlTemplate(const String &title, const String &content, const String &scripts = "")
    {
        String html = getProgmemString(ProgmemStrings::htmlHeader);
        html.replace("{TITLE}", title);
        html += getCommonStyles();
        html += getProgmemString(ProgmemStrings::htmlFooter);
        html.replace("{MENU}", generateMenu());
        html.replace("{CONTENT}", content);
        html.replace("{SCRIPTS}", scripts);
        return html;
    }

    void generateHomePage(AsyncWebServerRequest *request)
    {
        String deviceList;
        for (auto &data : DeviceInfo::deviceRegList)
        {
            data.name.toUpperCase();
            if (data.tid > 0)
            {
                deviceList += generateDeviceCard(data.tid, String(data.name));
            }
        }

        String scripts = getProgmemString(R"=====(
            <script>
            async function updateStatus(id) {
                const res = await fetch(`/ctlDevice?tid=${id}&action=status`, { method: 'POST' });
                const statusElement = document.getElementById(`device-status-${id}`);
                if (res.ok) {
                    const json = await res.json();
                    statusElement.innerText = json.status;
                } else {
                    statusElement.innerText = 'Erro';
                }
            }
            document.addEventListener('DOMContentLoaded', () => {
                const devices = [
        )=====");

        for (const auto &data : DeviceInfo::deviceRegList)
        {
            if (data.tid > 0)
            {
                scripts += "'" + String(data.tid) + "',";
            }
        }

        scripts += getProgmemString(R"=====(
                ];
                let currentIndex = 0;
                const updateNextDevice = () => {
                    if (currentIndex < devices.length) {
                        updateStatus(devices[currentIndex]);
                        currentIndex++;
                    } else {
                        currentIndex = 0;
                    }
                };
                setInterval(updateNextDevice, 1000);
            });
            </script>
        )=====");

        String content = "<header><h1>Dispositivos Conectados</h1></header>";
        content += "<div class='device-list'>" + deviceList + "</div>";

        request->send(200, "text/html", generateHtmlTemplate("Dispositivos Conectados", content, scripts));
    }

    // ... (outras funções permanecem semelhantes, mas usando as novas funções PROGMEM)

    void handleToggleDevice(AsyncWebServerRequest *request)
    {
        uint8_t tid = request->hasArg("tid") ? request->arg("tid").toInt() : 0xFF;
        String action = request->hasArg("action") ? request->arg("action") : "none";
        action.toLowerCase();

        const int16_t x = DeviceInfo::dataOf(tid);
        String status = getProgmemString(ProgmemStrings::unknownStatus);
        if (x >= 0)
        {
            const DeviceInfoData &data = DeviceInfo::deviceList[x];

            if (action == "toggle")
            {
                LoRaCom::sendCommand("gpio", "toggle", tid);
            }

            int timeDiff = DeviceInfo::getTimeDifferenceSeconds(data.lastSeenISOTime);
            bool isOffline = (timeDiff == -1) || (timeDiff > 60);
            status = isOffline ? getProgmemString(ProgmemStrings::offlineStatus) : (data.status.length() == 0 ? getProgmemString(ProgmemStrings::unknownStatus) : data.status);
        }
        respStatus(request, tid, status);
    }

    void respStatus(AsyncWebServerRequest *request, uint8_t tid, String status)
    {
        String response = "{ \"tid\": " + String(tid) +
                          ", \"status\": \"" + status + "\" }";
        request->send(200, "application/json", response);
    }

    void initWebServer(AsyncWebServer *ws)
    {
        if (ws == nullptr)
        {
            espServer = new AsyncWebServer(80);
        }
        else
        {
            espServer = ws;
        }

        // Setup server routes
        espServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                      { generateHomePage(request); });

        espServer->on("/ctlDevice", HTTP_POST, [](AsyncWebServerRequest *request)
                      { handleToggleDevice(request); });

        // Add other route handlers as needed
    }

    bool begin()
    {
        if (espServer != nullptr)
        {
            espServer->begin();
            return true;
        }
        return false;
    }

    void process()
    {
        // Process any periodic tasks for the web server
        // This can be left empty if no periodic processing is needed
        // or you can add periodic tasks here
    }

} // namespace HtmlServer

#endif
#endif