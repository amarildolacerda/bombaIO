#ifdef ESP8266
#include <ESP8266WebServer.h>
#include "logger.h"
#include "config.h"

ESP8266WebServer server(80);

void handleRootRequest()
{
    String html = "<!DOCTYPE html><html lang='en'>";
    html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Device Status</title></head><body>";
    html += "<h1>Device Status</h1>";
    html += "<p>Status: <span id='status'></span></p>";
    html += "<script>";
    html += "async function updateStatus() {";
    html += "  const res = await fetch('/status');";
    html += "  const json = await res.json();";
    html += "  document.getElementById('status').innerText = json.status;";
    html += "}";
    html += "window.onload = function() { updateStatus(); setInterval(updateStatus, 2000); };";
    html += "</script>";
    html += "<button onclick=\"toggleDevice()\">Toggle</button>";
    html += "<script>";
    html += "async function toggleDevice() {";
    html += "  await fetch('/toggle', { method: 'POST' });";
    html += "  const res = await fetch('/status');";
    html += "  const json = await res.json();";
    html += "  document.getElementById('status').innerText = json.status;";
    html += "}";
    html += "</script></body></html>";
    server.send(200, "text/html", html);
}

void handleToggleRequest()
{
    int currentState = digitalRead(Config::RELAY_PIN);
    digitalWrite(Config::RELAY_PIN, !currentState);
    Logger::log(LogLevel::INFO, "Relay toggled");
    server.send(200, "application/json", "{\"status\":\"" + String(digitalRead(Config::RELAY_PIN) == HIGH ? "ON" : "OFF") + "\"}");
}

void handleStatusRequest()
{
    Serial.print("Status: ");
    Serial.println(digitalRead(Config::RELAY_PIN));
    server.send(200, "application/json", "{\"status\":\"" + String(digitalRead(Config::RELAY_PIN) == HIGH ? "ON" : "OFF") + "\"}");
}

void initHtmlServer()
{
    server.on("/", HTTP_GET, handleRootRequest);
    server.on("/toggle", HTTP_POST, handleToggleRequest);
    server.on("/status", HTTP_GET, handleStatusRequest);
    server.begin();
    Logger::log(LogLevel::INFO, "HTML Server started");
}

void processHtmlServer()
{
    server.handleClient();
}

#endif