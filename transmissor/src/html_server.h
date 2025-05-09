#ifndef HTML_SERVER_H
#define HTML_SERVER_H

#include <WebServer.h>
#include <map>
#include <string>

namespace HtmlServer
{

    // ========== Web Server Implementations ==========
    String generateHtmlPage();
    void handleRootRequest();
    void handleStateRequest();
    void handleRevertRelayRequest();
    void handleTurnOnRelayRequest();
    void handleTurnOffRelayRequest();
    void handleDeviceListRequest();
    String generateDeviceListHtml();
    void initWebServer();
    String generateDeviceListHtml();

    void process();

} // namespace HtmlServer

extern WebServer server;
#endif // HTML_SERVER_H