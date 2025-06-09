#ifdef WS
#ifndef HTML_SERVER_H
#define HTML_SERVER_H

// #include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    void waitTimeout(const bool ateQueDiferente, const int timeout = 3000);

    void generateHtmlPage();
    void generateDeviceListHtmlPage();
    void handleToggleDevice();
    void initWebServer(AsyncWebServer *ws = nullptr);

    void process();
    bool begin();
    void respStatus(AsyncWebServerRequest *request, uint8_t tid, String status);
    void handleToggleDevice(AsyncWebServerRequest *request);

} // namespace HtmlServer

#endif

#endif // HTML_SERVER_H