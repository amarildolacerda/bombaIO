#include "html_server.h"
#include "logger.h"
#include "transmissor.h"
#include <Arduino.h>
#include "config.h"
#include "system_state.h"

namespace HtmlServer
{

    Server::Server(WebServer &server) : server(server) {}

    void Server::init()
    {
        server.on("/", HTTP_GET, [this]()
                  { handleRootRequest(); });
        server.on("/getState", HTTP_GET, [this]()
                  { handleStateRequest(); });
        server.on("/revertRelay", HTTP_POST, [this]()
                  { handleRevertRelayRequest(); });
        server.on("/turnOnRelay", HTTP_POST, [this]()
                  { handleTurnOnRelayRequest(); });
        server.on("/turnOffRelay", HTTP_POST, [this]()
                  { handleTurnOffRelayRequest(); });
        server.on("/devices", HTTP_GET, [this]()
                  { handleDeviceListRequest(); });
    }

    void Server::handleRootRequest()
    {
        server.send(200, "text/html", generateControlPageHtml().c_str());
    }

    void Server::handleStateRequest()
    {
        LoRaCom::sendCommand("get", "status", 0xFF);
        uint32_t start = millis();
        while (millis() - start < Config::COMMAND_TIMEOUT)
        {
            LoRaCom::processIncoming();
            if (systemState.getState() != "DESCONHECIDO")
            {
                break;
            }
        }
        server.send(200, "text/plain", systemState.getState().c_str());
    }

    void Server::handleRevertRelayRequest()
    {
        LoRaCom::sendCommand("gpio", "revert", 0xFF);
        server.send(200, "text/plain", "Comando de reversão enviado");
    }

    void Server::handleTurnOnRelayRequest()
    {
        LoRaCom::sendCommand("gpio", "on", 0xFF);
        server.send(200, "text/plain", "Comando para ligar o relé enviado");
    }

    void Server::handleTurnOffRelayRequest()
    {
        LoRaCom::sendCommand("gpio", "off", 0xFF);
        server.send(200, "text/plain", "Comando para desligar o relé enviado");
    }

    void Server::handleDeviceListRequest()
    {
        server.send(200, "text/html", generateDeviceListHtml().c_str());
    }

    std::string Server::generateControlPageHtml()
    {
        // Implementation of the HTML generation logic for the control page
        return "<html><body><h1>Controle TTGO</h1></body></html>";
    }

    std::string Server::generateDeviceListHtml()
    {
        // Implementation of the HTML generation logic for the device list page
        return "<html><body><h1>Lista de Dispositivos</h1></body></html>";
    }

} // namespace HtmlServer