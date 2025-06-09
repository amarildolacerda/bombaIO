#include "WiFiConn.h"

#ifdef ALEXA
// AlexaCallbackType WiFiConn::alexaCallbackFn = nullptr;
#endif

AsyncWebServer server(Config::WEBSERVER_PORT);
DNSServer dns;
WiFiConn wifiConn(&server, &dns);
