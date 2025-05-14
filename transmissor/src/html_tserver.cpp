#include "html_tserver.h"
#include "logger.h"
#include <Arduino.h>
#include "config.h"
#include "system_state.h"
#include "device_info.h"
#include "LoRaCom.h"
#include "prefers.h"
#ifndef __AVR__

#ifdef ESP32
#include "update.h"
#endif

#undef DISPLAY

#ifdef ESP32
WebServer espServer(Config::WEBSERVER_PORT);
#include "Espalexa.h"
#elif ESP8266
ESP8266WebServer espServer(Config::WEBSERVER_PORT);
#include "Espalexa.h"
#endif

namespace HtmlServer
{

    String getCommonStyles()
    {
        String styles = "  * { box-sizing: border-box; }";
        styles += "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }";
        styles += "  .container { max-width: 800px; margin: 0 auto; }";
        styles += "  h1 { text-align: center; color: #2c3e50; margin-bottom: 20px; }";
        styles += "  .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 25px; margin-bottom: 25px; }";
        styles += "  h2 { color: #3498db; margin-top: 0; border-bottom: 1px solid #eee; padding-bottom: 10px; }";
        styles += "  .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 15px; }";
        styles += "  button { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; transition: all 0.3s; flex: 1 1 120px; }";
        styles += "  button:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
        styles += "  button:active { transform: translateY(0); }";
        styles += "  .status { color: #7f8c8d; font-size: 14px; margin-top: 5px; }";
        styles += "  .btn-danger { background: #e74c3c; }";
        styles += "  .btn-danger:hover { background: #c0392b; }";
        styles += "  .btn-success { background: #2ecc71; }";
        styles += "  .btn-success:hover { background: #27ae60; }";
        styles += "  .btn-warning { background: #f39c12; }";
        styles += "  .btn-warning:hover { background: #d35400; }";
        styles += "  @media (max-width: 600px) { .button-group { flex-direction: column; } button { width: 100%; } }";
        return styles;
    }

    void generateHomePage()
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Dispositivos Conectados</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "  .device-card {";
        html += "    display: inline-block;";
        html += "    width: 200px;";
        html += "    margin: 10px;";
        html += "    padding: 15px;";
        html += "    border: 1px solid #ddd;";
        html += "    border-radius: 8px;";
        html += "    text-align: center;";
        html += "    background-color: #fff;";
        html += "    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);";
        html += "  }";
        html += "  .device-card h3 {";
        html += "    margin: 10px 0;";
        html += "    font-size: 18px;";
        html += "    color: #333;";
        html += "  }";
        html += "  .device-card button {";
        html += "    margin-top: 10px;";
        html += "    padding: 10px;";
        html += "    background-color: #3498db;";
        html += "    color: white;";
        html += "    border: none;";
        html += "    border-radius: 5px;";
        html += "    cursor: pointer;";
        html += "  }";
        html += "  .device-card button:hover {";
        html += "    background-color: #2980b9;";
        html += "  }";
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += "  <header>";
        html += "    <h1>Dispositivos Conectados</h1>";
        html += "  </header>";

        html += "  <div class='device-list'>";

        for (const auto &device : DeviceInfo::deviceList)
        {
            DeviceInfoData data = device.second.second;
            data.name.toUpperCase();
            if (data.tid > 0)
            {
                html += "    <div class='device-card'>";
                html += "      <h3>";
                html += "      <div style='cursor:pointer;' onclick=\"window.location='/device?tid=" + String(data.tid) + "'\">" + String(data.name) + "</div></h3>";
                html += "     <hr/> <p id='device-status-" + String(data.tid) + "' style=\"cursor:pointer;\" onclick=\"fetch('/controlDevice?tid=" + String(data.tid) + "&action=toggle', {method: 'POST'}).then(() => updateStatus(" + String(data.tid) + "));\">...</p>";
                html += "    </div>";
            }
        }

        html += "  </div>";
        html += "</div>";

        html += "<script>";
        html += "async function updateStatus(id) {";
        html += "  const res = await fetch(`/controlDevice?tid=${id}&action=status`, { method: 'POST' });";
        html += "  const statusElement = document.getElementById(`device-status-${id}`);";
        html += "  if (res.ok) {";
        html += "    const json = await res.json();";
        html += "    statusElement.innerText = json.status;";
        html += "  } else {";
        html += "    statusElement.innerText = 'Erro';";
        html += "  }";
        html += "}";
        html += "document.addEventListener('DOMContentLoaded', () => {";
        html += "  const devices = [";
        for (const auto &device : DeviceInfo::deviceList)
        {
            DeviceInfoData data = device.second.second;
            if (data.tid > 0)
            {
                html += "'" + String(data.tid) + "',";
            }
        }
        html += "];";
        html += "  devices.forEach(id => updateStatus(id));";
        html += "  devices.forEach(id => setInterval(() => updateStatus(id), 1000));";
        html += "});";
        html += "</script>";

        html += "</body></html>";

        espServer.send(200, "text/html", html.c_str());
    }

    void generateDeviceDetailsPage(uint8_t tid)
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Detalhes do Dispositivo</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += "  <header>";
        html += "    <h1>Detalhes do Dispositivo</h1>";
        html += "  </header>";

        const auto &device = DeviceInfo::deviceList[tid];
        const DeviceInfoData &data = device.second;

        html += "  <div class='card'>";
        html += "    <h2>" + data.name + "</h2>";
        html += "    <p>RSSI: " + String(data.rssi) + " dBm</p>";
        html += "    <p>Última Atualização: " + data.lastSeenISOTime + "</p>";
        html += "    <p id='device-status'>Estado: Carregando...</p>";
        html += "<script>";
        html += "async function updateStatus() {";
        html += "  const res = await fetch(`/controlDevice?tid=" + String(data.tid) + "&action=status`, { method: 'POST' });";
        html += "  if (res.ok) {";
        html += "    const json = await res.json();";
        html += "    document.getElementById('device-status').innerText = 'Estado: ' + json.status;";
        html += "  } else {";
        html += "    document.getElementById('device-status').innerText = 'Estado: Erro ao obter';";
        html += "  }";
        html += "}";
        html += "updateStatus();";
        html += "setInterval(updateStatus, 1000);";
        html += "</script>";
        html += "    <button onclick=\"toggleDevice('" + String(data.tid) + "')\" class='btn-warning'>Alternar Estado</button>";
        html += "  </div>";

        html += "  <a href='/' class='btn-info'>Voltar</a>";
        html += "</div>";

        html += "<script>";
        html += "async function toggleDevice(id) {";
        html += "  await fetch(`/controlDevice?tid=${id}&action=toggle`, { method: 'POST' });";
        html += "  location.reload();";
        html += "}";
        html += "</script>";

        html += "</body></html>";

        espServer.send(200, "text/html", html.c_str());
    }

    void handleRootRequest()
    {
        generateHomePage();
    }

    void handleDeviceDetailsRequest()
    {
        uint8_t tid = espServer.hasArg("tid") ? espServer.arg("tid").toInt() : 0xFF;
        if (DeviceInfo::deviceList.find(tid) != DeviceInfo::deviceList.end())
        {
            generateDeviceDetailsPage(tid);
        }
        else
        {
            espServer.send(404, "text/plain", "Dispositivo não encontrado");
        }
    }

    Espalexa *alexa = nullptr;
    void initWebServer(Espalexa *espalexa)
    {
        alexa = espalexa;
        espServer.on("/", HTTP_GET, handleRootRequest);
        espServer.on("/device", HTTP_GET, handleDeviceDetailsRequest);
        espServer.on("/controlDevice", HTTP_POST, handleToggleDevice);
        espServer.on("/reset", HTTP_GET, []()
                     { DeviceInfo::deviceRegList.clear(); 
                        Prefers::saveRegs();
         espServer.send(404, "text/plain", "OK"); });
        // Handler catch-all para qualquer rota desconhecida
        espServer.onNotFound([espalexa]()
                             {
                                Serial.print("NOTFOUND:"+espServer.uri());
            if (!espalexa->handleAlexaApiCall(espServer.uri(), espServer.arg(0))) {
                Serial.print("NENHUM ALEXA");
                espServer.send(404, "text/plain", "Not found");
            } });
        espServer.on("/", HTTP_ANY, []()
                     {
                        Serial.print("ANY:"+espServer.uri());
            if (espServer.method() != HTTP_GET) {
                espServer.send(404, "text/plain", "Not found");
            } });
    }
    void begin()
    {
        alexa->begin(&espServer);
        espServer.begin();
    }
    void handleToggleDevice()
    {
        uint8_t tid = espServer.hasArg("tid") ? espServer.arg("tid").toInt() : 0xFF;
        String action = espServer.hasArg("action") ? espServer.arg("action") : "none";
        action.toLowerCase();
        const auto &device = DeviceInfo::deviceList[tid];
        const DeviceInfoData &data = device.second;
        if (action == "toggle")
            LoRaCom::sendCommand("gpio", "toggle", tid);
        else if (action == "status")
        {
            String response = "{ \"tid\": " + String(data.tid) +
                              ", \"status\": \"" + data.status + "\" }";
            espServer.send(200, "application/json", response);
        }
    }
    void process()
    {
        espServer.handleClient();
    }
}
#endif