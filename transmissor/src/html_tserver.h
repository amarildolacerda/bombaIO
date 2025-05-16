#ifndef HTML_SERVER_H
#define HTML_SERVER_H

#ifndef __AVR__
#ifdef ESP32
#include <WebServer.h>
#elif ESP8266
#include <ESP8266WebServer.h>
// extern ESP8266WebServer server;
#endif

namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    void waitTimeout(const bool ateQueDiferente, const int timeout = 3000);

    void generateHtmlPage();
    void generateDeviceListHtmlPage();
    void handleToggleDevice();
    void initWebServer(WebServer *ws = nullptr);

    void process();
    void begin();

} // namespace HtmlServer

#endif

#endif // HTML_SERVER_H