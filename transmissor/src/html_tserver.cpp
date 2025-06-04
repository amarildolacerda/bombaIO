#ifdef WS
#include "html_tserver.h"
#include "logger.h"
#include <Arduino.h>
#include "config.h"
#include "display_manager.h"
#include "system_state.h"
#include "device_info.h"
#include "LoRaCom.h"
#include "prefers.h"
#ifndef __AVR__

#ifdef ESP32
#include <Update.h>
#endif

namespace HtmlServer
{
    AsyncWebServer *espServer = nullptr;

    String getCommonStyles()
    {
        String styles = "  * { box-sizing: border-box; }";
        styles += "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }";
        styles += "  .container { max-width: 800px; margin: 0 auto; }";
        styles += "  h1 { text-align: center; color: #2c3e50; margin-bottom: 20px; }";
        styles += "  .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 25px; margin-bottom: 25px; }";
        styles += "  h2 { color: #3498db; margin-top: 0; border-bottom: 1px solid #eee; padding-bottom: 10px; }";
        styles += "  .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 15px; }";
        styles += "  button, .btn { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; transition: all 0.3s; flex: 1 1 120px; text-decoration: none; display: inline-block; text-align: center; }";
        styles += "  button:hover, .btn:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
        styles += "  button:active, .btn:active { transform: translateY(0); }";
        styles += "  .status { color: #7f8c8d; font-size: 14px; margin-top: 5px; }";
        styles += "  .btn-danger { background: #e74c3c; }";
        styles += "  .btn-danger:hover { background: #c0392b; }";
        styles += "  .btn-success { background: #2ecc71; }";
        styles += "  .btn-success:hover { background: #27ae60; }";
        styles += "  .btn-warning { background: #f39c12; }";
        styles += "  .btn-warning:hover { background: #d35400; }";
        styles += "  .btn-info { background: #3498db; }";
        styles += "  .btn-info:hover { background: #2980b9; }";
        styles += "  @media (max-width: 600px) { .button-group { flex-direction: column; } button, .btn { width: 100%; } }";
        styles += "  .menu { background: #2c3e50; padding: 15px; margin-bottom: 20px; border-radius: 8px; display: flex; flex-wrap: wrap; gap: 10px; }";
        styles += "  .menu-item { color: white; text-decoration: none; padding: 8px 12px; border-radius: 4px; }";
        styles += "  .menu-item:hover { background: #3498db; }";
        styles += "  .upload-form { margin-top: 20px; }";
        styles += "  .progress { margin-top: 20px; width: 100%; background-color: #f1f1f1; border-radius: 4px; overflow: hidden; }";
        styles += "  .progress-bar { width: 0%; height: 30px; background-color: #4CAF50; text-align: center; line-height: 30px; color: white; transition: width 0.3s; }";
        styles += "  .device-card { display: inline-block; width: 200px; margin: 10px; padding: 15px; border: 1px solid #ddd; border-radius: 8px; text-align: center; background-color: #fff; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }";
        styles += "  .device-card h3 { margin: 10px 0; font-size: 18px; color: #333; }";
        styles += "  .device-card button { margin-top: 10px; padding: 10px; background-color: #3498db; color: white; border: none; border-radius: 5px; cursor: pointer; }";
        styles += "  .device-card button:hover { background-color: #2980b9; }";
        return styles;
    }

    String generateMenu()
    {
        // Menu
        String html = "<div class='menu'>";
        html += "<a href='/' class='menu-item'>Home</a>";
        html += "<a href='/ota' class='menu-item'>OTA Update</a>";
        //  html += "<a href='/reset' class='menu-item'>Reset Devices</a>";
        html += "<a href='/restart' class='menu-item'>Reiniciar</a>";
        html += "<a href='/discovery' class='menu-item' id='discovery-btn'>Novo</a>";
        html += "</div>";
        // Adiciona o script para controlar o modo de descoberta
        html += "<script>"
                "let discoveryActive = false;"
                "let discoveryTimeout;"
                "function toggleDiscovery() {"
                "  discoveryActive = !discoveryActive;"
                "  const btn = document.getElementById('discovery-btn');"
                "  if (discoveryActive) {"
                "    btn.style.backgroundColor = '#2ecc71';" // Verde quando ativo
                "    fetch('/discovery?enable=1', {method: 'POST'});"
                "    discoveryTimeout = setTimeout(() => {"
                "      discoveryActive = false;"
                "      btn.style.backgroundColor = '';"
                "    }, 60000);" // 1 minutos
                "  } else {"
                "    clearTimeout(discoveryTimeout);"
                "    btn.style.backgroundColor = '';"
                "    fetch('/discovery?enable=0', {method: 'POST'});"
                "  }"
                "}"
                "document.getElementById('discovery-btn').onclick = function(e) {"
                "  e.preventDefault();"
                "  toggleDiscovery();"
                "  return false;"
                "};"
                "</script>";
        return html;
    }

    void generateHomePage(AsyncWebServerRequest *request)
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Dispositivos Conectados</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += generateMenu();

        html += "  <header>";
        html += "    <h1>Dispositivos Conectados</h1>";
        html += "  </header>";
        html += "  <div class='device-list'>";

        for (auto &data : DeviceInfo::deviceRegList)
        {
            data.name.toUpperCase();
            if (data.tid > 0)
            {
                html += "    <div class='device-card'>";
                html += "      <h3>";
                html += "      <div style='cursor:pointer;' onclick=\"window.location='/device?tid=" + String(data.tid) + "'\">" + String(data.name) + "</div></h3>";
                html += "     <hr/> <p id='device-status-" + String(data.tid) + "' style=\"cursor:pointer;\" onclick=\"fetch('/ctlDevice?tid=" + String(data.tid) + "&action=toggle', {method: 'POST'}).then(() => updateStatus(" + String(data.tid) + "));\">...</p>";
                html += "    </div>";
            }
        }

        html += "  </div>";
        html += "</div>";

        html += "<script>";
        html += "async function updateStatus(id) {";
        html += "  const res = await fetch(`/ctlDevice?tid=${id}&action=status`, { method: 'POST' });";
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
        for (const auto &data : DeviceInfo::deviceRegList)
        {
            if (data.tid > 0)
            {
                html += "'" + String(data.tid) + "',";
            }
        }

        html += "];";
        html += "  let currentIndex = 0;"; // Adiciona um contador
        html += "  const updateNextDevice = () => {";
        html += "    if (currentIndex < devices.length) {";
        html += "      updateStatus(devices[currentIndex]);";
        html += "      currentIndex++;"; // Incrementa o contador
        html += "    } else {";
        html += "      currentIndex = 0;"; // Reinicia o contador
        html += "    }";
        html += "  };";
        html += "  setInterval(updateNextDevice, 1000);"; // Atualiza um dispositivo a cada segundo
        html += "});";

        html += "</script>";

        html += "</body></html>";

        request->send(200, "text/html", html.c_str());
    }

    void generateDeviceDetailsPage(AsyncWebServerRequest *request, uint8_t tid)
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

        generateMenu();

        const uint8_t idx = DeviceInfo::indexOf(tid);
        if (idx >= 0)
        {
            const DeviceRegData &rdata = DeviceInfo::deviceRegList[idx];
            const uint16_t didx = DeviceInfo::dataOf(tid);
            DeviceInfoData data;
            if (didx >= 0)
                data = DeviceInfo::deviceList[didx];

            html += "  <div class='card'>";
            html += "    <h2>" + rdata.name + "</h2>";
            html += "    <p>RSSI: " + ((didx >= 0) ? String(data.rssi) : "") + " dBm</p>";
            html += "    <p>Última Atualização: " + ((didx >= 0) ? String(DeviceInfo::getTimeDifferenceSeconds(data.lastSeen)) : "") + "s</p>";
            html += "    <p id='device-status'>Estado: Carregando...</p>";
            html += "<script>";
            html += "async function updateStatus() {";
            html += "  const res = await fetch(`/ctlDevice?tid=" + String(rdata.tid) + "&action=status`, { method: 'POST' });";
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
            html += "    <button onclick=\"toggleDevice('" + String(rdata.tid) + "')\" class='btn-warning'>Alternar Estado</button>";
            html += "  </div>";
        }
        html += "  <a href='/' class='btn-info'>Voltar</a>";
        html += "</div>";

        if (idx >= 0)
        {
            html += "<script>";
            html += "async function toggleDevice(id) {";
            html += "  await fetch(`/ctlDevice?tid=${id}&action=toggle`, { method: 'POST' });";
            html += "  location.reload();";
            html += "}";
            html += "</script>";
        }
        html += "</body></html>";

        request->send(200, "text/html", html.c_str());
    }

    void generateOTAPage(AsyncWebServerRequest *request)
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>OTA Update</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";

        generateMenu();

        html += "  <div class='card'>";
        html += "    <h2>OTA Update</h2>";
        html += "    <form class='upload-form' method='POST' action='/update' enctype='multipart/form-data'>";
        html += "      <input type='file' name='update' accept='.bin' style='margin-bottom: 15px; width: 100%; padding: 10px;'>";
        html += "      <button type='submit' class='btn-success'>Iniciar Atualização</button>";
        html += "    </form>";
        html += "    <div class='progress' style='display:none;'>";
        html += "      <div class='progress-bar' id='progress'>0%</div>";
        html += "    </div>";
        html += "  </div>";
        html += "</div>";

        html += "<script>";
        html += "document.querySelector('form').addEventListener('submit', function(e) {";
        html += "  e.preventDefault();";
        html += "  var formData = new FormData(this);";
        html += "  var xhr = new XMLHttpRequest();";
        html += "  xhr.open('POST', '/update', true);";
        html += "  xhr.upload.onprogress = function(e) {";
        html += "    if (e.lengthComputable) {";
        html += "      var percent = Math.round((e.loaded / e.total) * 100);";
        html += "      document.querySelector('.progress').style.display = 'block';";
        html += "      document.getElementById('progress').style.width = percent + '%';";
        html += "      document.getElementById('progress').innerHTML = percent + '%';";
        html += "    }";
        html += "  };";
        html += "  xhr.onload = function() {";
        html += "    if (this.status == 200) {";
        html += "      alert('Atualização concluída com sucesso! O dispositivo irá reiniciar.');";
        html += "      setTimeout(function() { window.location.href = '/'; }, 2000);";
        html += "    } else {";
        html += "      alert('Erro na atualização: ' + this.responseText);";
        html += "      document.querySelector('.progress').style.display = 'none';";
        html += "    }";
        html += "  };";
        html += "  xhr.send(formData);";
        html += "});";
        html += "</script>";

        html += "</body></html>";

        request->send(200, "text/html", html.c_str());
    }

    void handleRootRequest(AsyncWebServerRequest *request)
    {
        generateHomePage(request);
    }

    void handleDeviceDetailsRequest(AsyncWebServerRequest *request)
    {
        uint8_t tid = request->hasArg("tid") ? request->arg("tid").toInt() : 0xFF;
        uint8_t idx = DeviceInfo::indexOf(tid);
        if (idx >= 0)
        {
            generateDeviceDetailsPage(request, tid);
        }
        else
        {
            request->send(404, "text/plain", "Dispositivo não encontrado");
        }
    }

    void respStatus(AsyncWebServerRequest *request, uint8_t tid, String status)
    {
        String response = "{ \"tid\": " + String(tid) +
                          ", \"status\": \"" + status + "\" }";
        request->send(200, "application/json", response);
    }

    void handleToggleDevice(AsyncWebServerRequest *request)
    {
        uint8_t tid = request->hasArg("tid") ? request->arg("tid").toInt() : 0xFF;
        String action = request->hasArg("action") ? request->arg("action") : "none";
        action.toLowerCase();

        const int16_t x = DeviceInfo::dataOf(tid);

        if (action == "toggle")
        { // o tid é o handle do dispositivo, nao depende de procura
            // pode ser que o terminal nao esta respondendo, mas mesh o alcance
            LoRaCom::sendCommand("gpio", "toggle", tid);
        }
        if (action == "on")
        { // o tid é o handle do dispositivo, nao depende de procura
            // pode ser que o terminal nao esta respondendo, mas mesh o alcance
            LoRaCom::sendCommand("gpio", "on", tid);
        }
        if (action == "off")
        { // o tid é o handle do dispositivo, nao depende de procura
            // pode ser que o terminal nao esta respondendo, mas mesh o alcance
            LoRaCom::sendCommand("gpio", "off", tid);
        }

        String status = "Aguardando";
        if (x >= 0)
        {
            const DeviceInfoData &data = DeviceInfo::deviceList[x];

            // LoRaCom::sendCommand("status", "get", tid); // muitas chamadas redundandtes.
            int timeDiff = DeviceInfo::getTimeDifferenceSeconds(data.lastSeen);
            bool isOffline = (timeDiff == -1) || (timeDiff > 60 * 2);
            status = isOffline ? "Não Responde" : (data.value.length() == 0 ? "???" : data.value);
        }
        respStatus(request, tid, status);
    }

    void doOTAUpdate(AsyncWebServerRequest *request)
    {
    }

    void initWebServer(AsyncWebServer *server)
    {
        espServer = server;

        // Rotas principais
        espServer->on("/", HTTP_GET, handleRootRequest);
        espServer->on("/device", HTTP_GET, handleDeviceDetailsRequest);
        espServer->on("/ctlDevice", HTTP_POST, [](AsyncWebServerRequest *request)
                      { handleToggleDevice(request); });
        espServer->on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            DeviceInfo::deviceRegList.clear(); 
            DeviceInfo::deviceList.clear();
            Prefers::saveRegs();
            displayManager.showMessage("Reset no dispositivos");
//            request->send(200, "text/plain", "OK"); 
            request->redirect("/");
            delay(100);
            ESP.restart(); });

        espServer->on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
                      {
            request->redirect ("/");
            delay(1000);
                        ESP.restart(); });

        espServer->on("/discovery", HTTP_POST, [](AsyncWebServerRequest *request)
                      {
        if (request->hasArg("enable")) {
            systemState.setDiscovery(request->arg("enable") == "1",60000);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        } });

        // Rotas OTA
        espServer->on("/ota", HTTP_GET, generateOTAPage);

        espServer->on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
                      { doOTAUpdate(request); });
    }

    bool begin()
    {
        espServer->begin();
        Serial.println("HTTP server started");
        return true;
    }

    void process()
    {
        // espServer-> ;
    }
}

#endif
#endif