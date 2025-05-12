#ifndef HTML_SERVER_H
#define HTML_SERVER_H

#ifndef __AVR__
namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    void waitTimeout(const bool ateQueDiferente, const int timeout = 3000);

    void generateHtmlPage();
    void generateDeviceListHtmlPage();
    void handleRootRequest();
    void handleStateRequest();
    void handleRevertRelayRequest();
    void handleTurnOnRelayRequest();
    void handleTurnOffRelayRequest();
    void handleDeviceListRequest();
    void initWebServer();

    void process();

} // namespace HtmlServer

#ifdef ESP32
#include <WebServer.h>
extern WebServer server;
#elif ESP8266

#include <ESP8266WebServer.h>
extern ESP8266WebServer server;
#endif
#endif

#endif // HTML_SERVER_H