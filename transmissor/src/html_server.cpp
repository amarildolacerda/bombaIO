#include "html_server.h"
#include "logger.h"
#include <Arduino.h>
#include "config.h"
#include "system_state.h"
#include "device_info.h"
#include "LoRaCom.h"

#ifndef __AVR__ // Start of AVR check

#ifdef ESP32 // Start of ESP32-specific code
#include "update.h"
#endif // End of ESP32-specific code

#undef DISPLAY // Avoid macro redefinition warning

// Add the correct external declaration for the server object
#ifdef ESP32 // Start of ESP32-specific server declaration
WebServer server(Config::WEBSERVER_PORT);
#elif ESP8266
ESP8266WebServer server(Config::WEBSERVER_PORT);
#endif // End of ESP32-specific server declaration

namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    String getCommonStyles()
    {
        String styles = "  * { box-sizing: border-box; }";
        styles += "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }";
        styles += "  .container { max-width: 800px; margin: 0 auto; }";
        styles += "  h1 { text-align: center; color: #2c3e50; margin-bottom: 20px; }";
        styles += "  table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }";
        styles += "  th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }";
        styles += "  th { background-color: #3498db; color: white; }";
        styles += "  tr:hover { background-color: #f1f1f1; }";
        styles += "  a { display: inline-block; padding: 10px 15px; background-color: #3498db; color: white; text-decoration: none; border-radius: 5px; text-align: center; }";
        styles += "  a:hover { background-color: #2980b9; }";
        styles += "  .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 25px; margin-bottom: 25px; }";
        styles += "  h2 { color: #3498db; margin-top: 0; border-bottom: 1px solid #eee; padding-bottom: 10px; }";
        styles += "  .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 15px; }";
        styles += "  button { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; transition: all 0.3s; flex: 1 1 120px; }";
        styles += "  button:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
        styles += "  button:active { transform: translateY(0); }";
        styles += "  #stateInfo { background: #f8f9fa; padding: 15px; border-radius: 6px; margin: 15px 0; font-weight: bold; border-left: 4px solid #3498db; }";
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

    void generateHtmlPage()
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Controle TTGO</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += "  <header>";
        html += "    <h1>Controle TTGO</h1>";
        html += "    <p>Sistema de controle remoto</p>";
        html += "  </header>";

        html += "  <div class='card'>";
        html += "    <h2>Controle do Relé</h2>";
        html += "    <div class='button-group'>";
        html += "      <button onclick='getState()' class='btn-info'>Atualizar Estado</button>";
        html += "      <button onclick='turnOnRelay()' class='btn-success'>Ligar Relé</button>";
        html += "      <button onclick='turnOffRelay()' class='btn-danger'>Desligar Relé</button>";
        html += "      <button onclick='revertRelay()' class='btn-warning'>Inverter Relé</button>";
        html += "    </div>";
        html += "    <div id='stateInfo'>Estado atual: " + systemState.getState() + "</div>";
        html += "    <div class='status'>Última atualização: " + DeviceInfo::getISOTime() + "</div>";
        html += "    <div class='status'>RSSI Atual: " + String(LoRaCom::packetRssi()) + " dBm</div>";
        html += "  </div>";

        html += "  <div class='card'>";
        html += "    <h2>Gerenciamento do Sistema</h2>";
        html += "    <div class='button-group'>";
        html += "      <a href='/ota'><button class='btn-info'>Atualização OTA</button></a>";
        html += "      <button onclick='location.reload()' class='btn-warning'>Recarregar Página</button>";
        html += "      <a href='/devices'><button class='btn-info'>Lista de Dispositivos</button></a>";
        html += "    </div>";
        html += "  </div>";
        html += "</div>";

        html += "<script>";
        html += "async function showAlert(message, isError = false) {";
        html += "  const alertBox = document.createElement('div');";
        html += "  alertBox.style.position = 'fixed';";
        html += "  alertBox.style.top = '20px';";
        html += "  alertBox.style.left = '50%';";
        html += "  alertBox.style.transform = 'translateX(-50%)';";
        html += "  alertBox.style.padding = '15px 25px';";
        html += "  alertBox.style.background = isError ? '#e74c3c' : '#2ecc71';";
        html += "  alertBox.style.color = 'white';";
        html += "  alertBox.style.borderRadius = '5px';";
        html += "  alertBox.style.boxShadow = '0 3px 10px rgba(0,0,0,0.2)';";
        html += "  alertBox.style.zIndex = '1000';";
        html += "  alertBox.textContent = message;";
        html += "  document.body.appendChild(alertBox);";
        html += "  setTimeout(() => alertBox.remove(), 3000);";
        html += "}";

        html += "async function fetchData(url, method = 'GET') {";
        html += "  try {";
        html += "    document.getElementById('stateInfo').textContent = 'Carregando...';";
        html += "    const response = await fetch(url, { method });";
        html += "    if (!response.ok) throw new Error('Erro na requisição');";
        html += "    return await response.text();";
        html += "  } catch (error) {";
        html += "    showAlert(error.message, true);";
        html += "    throw error;";
        html += "  }";
        html += "}";

        html += "async function getState() {";
        html += "  try {";
        html += "    const state = await fetchData('/getState');";
        html += "    document.getElementById('stateInfo').textContent = 'Estado atual: ' + state;";
        html += "  } catch {}";
        html += "}";

        html += "async function turnOnRelay() {";
        html += "  try {";
        html += "    const message = await fetchData('/turnOnRelay', 'POST');";
        html += "    showAlert('Relé ligado: ' + message);";
        html += "    await getState();";
        html += "  } catch {}";
        html += "}";

        html += "async function turnOffRelay() {";
        html += "  try {";
        html += "    const message = await fetchData('/turnOffRelay', 'POST');";
        html += "    showAlert('Relé desligado: ' + message);";
        html += "    await getState();";
        html += "  } catch {}";
        html += "}";

        html += "async function revertRelay() {";
        html += "  try {";
        html += "    const message = await fetchData('/revertRelay', 'POST');";
        html += "    showAlert('Relé invertido: ' + message);";
        html += "    await getState();";
        html += "  } catch {}";
        html += "}";
        html += "</script>";
        html += "</body></html>";

        server.send(200, "text/html", html.c_str());
    }

    void handleRootRequest()
    {
        generateHtmlPage();
    }

    void handleStateRequest()
    {
        // Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleStateRequest");
        LoRaCom::sendCommand("get", "status", 0xFF);

        uint32_t start = millis();
        while (millis() - start < Config::COMMAND_TIMEOUT)
        {
            LoRaCom::handle();
            if (systemState.getState() != "DESCONHECIDO")
            {
                break;
            }
        }

        server.send(200, "text/plain", systemState.getState());
        // Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleStateRequest");
    }

    void handleRevertRelayRequest()
    {
        //  Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleRevertRelayRequest");
        LoRaCom::sendCommand("gpio", "toogle", 0xFF);
        server.send(200, "text/plain", "Comando de reversão enviado");
        //  Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleRevertRelayRequest");
    }

    void handleTurnOnRelayRequest()
    {
        //  Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleTurnOnRelayRequest");
        LoRaCom::sendCommand("gpio", "on", 0xFF);
        server.send(200, "text/plain", "Comando para ligar o relé enviado");
        //  Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleTurnOnRelayRequest");
    }

    void handleTurnOffRelayRequest()
    {
        // Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleTurnOffRelayRequest");
        LoRaCom::sendCommand("gpio", "off", 0xFF);
        server.send(200, "text/plain", "Comando para desligar o relé enviado");
        //  Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleTurnOffRelayRequest");
    }

    void handleDeviceListRequest()
    {
        generateDeviceListHtmlPage();
    }

    void handleOtaPageRequest()
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>OTA Update</title>";
        html += "<style>";
        html += "  body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }";
        html += "  .container { max-width: 600px; margin: 50px auto; padding: 20px; background: #fff; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }";
        html += "  h1 { text-align: center; color: #333; }";
        html += "  .upload-container { border: 2px dashed #3498db; border-radius: 10px; padding: 20px; text-align: center; background-color: #f9f9f9; cursor: pointer; transition: background-color 0.3s; }";
        html += "  .upload-container:hover { background-color: #eaf6ff; }";
        html += "  .upload-container input[type='file'] { display: none; }";
        html += "  .upload-container p { margin: 0; color: #7f8c8d; }";
        html += "  .file-name { margin-top: 10px; font-weight: bold; color: #2c3e50; }";
        html += "  .btn-success { display: block; width: 100%; padding: 10px; margin-top: 20px; background-color: #28a745; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
        html += "  .btn-success:hover { background-color: #218838; }";
        html += "  .btn-warning { display: block; width: 100%; padding: 10px; margin-top: 10px; background-color: #ffc107; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; text-align: center; text-decoration: none; }";
        html += "  .btn-warning:hover { background-color: #e0a800; }";
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += "<h1>Atualização OTA</h1>";
        html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
        html += "  <label class='upload-container'>";
        html += "    <input type='file' name='firmware' accept='.bin' required onchange='displayFileName(this)'>";
        html += "    <p>Arraste e solte o arquivo aqui ou clique para selecionar</p>";
        html += "    <span class='file-name' id='file-name'>Nenhum arquivo selecionado</span>";
        html += "  </label>";
        html += "  <button type='submit' class='btn-success'>Enviar Firmware</button>";
        html += "</form>";
        html += "<a href='/' class='btn-warning'>Voltar</a>";
        html += "</div>";
        html += "<script>";
        html += "  function displayFileName(input) {";
        html += "    const fileName = input.files[0] ? input.files[0].name : 'Nenhum arquivo selecionado';";
        html += "    document.getElementById('file-name').textContent = fileName;";
        html += "  }";
        html += "</script>";
        html += "</body></html>";

        server.send(200, "text/html", html.c_str());
    }

    void handleFirmwareUpload()
    {
#ifdef ESP32
        HTTPUpload &upload = server.upload();

        if (upload.status == UPLOAD_FILE_START)
        {
            Serial.printf("Iniciando upload: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (Update.end(true))
            {
                Serial.printf("Upload concluído: %s\n", upload.filename.c_str());
            }
            else
            {
                Update.printError(Serial);
            }
        }
#endif
    }

    void initWebServer()
    {
        Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initWebServer");
        server.on("/", HTTP_GET, handleRootRequest);
        server.on("/getState", HTTP_GET, handleStateRequest);
        server.on("/revertRelay", HTTP_POST, handleRevertRelayRequest);
        server.on("/turnOnRelay", HTTP_POST, handleTurnOnRelayRequest);
        server.on("/turnOffRelay", HTTP_POST, handleTurnOffRelayRequest);
        server.on("/devices", HTTP_GET, handleDeviceListRequest);
        server.on("/ota", HTTP_GET, handleOtaPageRequest);
        server.on("/update", HTTP_POST, []()
                  {
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart(); }, handleFirmwareUpload);

        server.begin();

        Logger::log(LogLevel::INFO, "Servidor HTTP iniciado");
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWebServer");
    }

    // Function to generate the device list HTML
    void generateDeviceListHtmlPage()
    {
        String html = "<!DOCTYPE html><html lang='pt-BR'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>Lista de Dispositivos</title>";
        html += "<style>";
        html += getCommonStyles();
        html += "</style>";
        html += "<script>";
        html += "function toggleRefresh(checkbox) {";
        html += "  localStorage.setItem('refreshEnabled', checkbox.checked);";
        html += "  if (checkbox.checked) {";
        html += "    setInterval(() => { location.reload(); }, 5000);";
        html += "  }";
        html += "}";
        html += "window.onload = function() {";
        html += "  const refreshEnabled = localStorage.getItem('refreshEnabled') === 'true';";
        html += "  const checkbox = document.getElementById('refreshCheckbox');";
        html += "  checkbox.checked = refreshEnabled;";
        html += "  if (refreshEnabled) {";
        html += "    setInterval(() => { location.reload(); }, 5000);";
        html += "  }";
        html += "}";
        html += "</script>";
        html += "</head>";
        html += "<body>";
        html += "<div class='card'>";
        html += "<h1>Dispositivos Registrados</h1>";
        html += "<label><input type='checkbox' id='refreshCheckbox' onclick='toggleRefresh(this)'> Atualizar automaticamente a cada 5 segundos</label>";
        html += "<table>";
        html += "<tr><th>Hora</th> <th>ID</th><th>Evento</th><th>Valor</th><th>RSSI</th></tr>";
        for (const auto &device : DeviceInfo::deviceList)
        {
            DeviceInfoData data = device.second.second;
            html += "<tr><td>" + data.lastSeenISOTime + "</td> <td>" + String(data.tid) + "</td><td>" + String(data.event.c_str()) + "</td><td>" + String(data.value.c_str()) + "</td><td>" + String(data.rssi) + "</td></tr>";
        }
        html += "</table>";
        html += "<a href='/'>Voltar</a>";
        html += "</div>";
        html += "</body></html>";

        server.send(200, "text/html", html.c_str());
    }

    void process()
    {
        server.handleClient();
    }

} // namespace HtmlServer
#endif // End of AVR check