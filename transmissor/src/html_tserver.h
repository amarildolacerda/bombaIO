#ifndef HTML_SERVER_H
#define HTML_SERVER_H

#ifndef __AVR__

#include "Espalexa.h"
namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    void waitTimeout(const bool ateQueDiferente, const int timeout = 3000);

    void generateHtmlPage();
    void generateDeviceListHtmlPage();
    void handleToggleDevice();
    void initWebServer(Espalexa *espalexa = nullptr);

    void process();
    void begin();

} // namespace HtmlServer

#ifdef ESP32
#include <WebServer.h>
extern WebServer espServer;
#elif ESP8266

#include <ESP8266WebServer.h>
extern ESP8266WebServer server;
#endif
#endif

#endif // HTML_SERVER_H