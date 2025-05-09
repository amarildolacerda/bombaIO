#ifndef HTML_SERVER_H
#define HTML_SERVER_H

#include <WebServer.h>
#include <map>
#include <string>

namespace HtmlServer
{

    class Server
    {
    public:
        Server(WebServer &server);

        void init();
        void handleRootRequest();
        void handleStateRequest();
        void handleRevertRelayRequest();
        void handleTurnOnRelayRequest();
        void handleTurnOffRelayRequest();
        void handleDeviceListRequest();

    private:
        WebServer &server;
        std::map<uint8_t, std::string> deviceList;

        std::string generateControlPageHtml();
        std::string generateDeviceListHtml();
    };

} // namespace HtmlServer

#endif // HTML_SERVER_H