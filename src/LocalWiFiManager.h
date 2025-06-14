#ifndef LOCAL_WIFI_MANAGER_H
#define LOCAL_WIFI_MANAGER_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Ticker.h>
#include <DNSServer.h>

class WiFiManager
{
private:
    AsyncWebServer *server;
    DNSServer *dns;
    bool isInConfigurationMode;
    String ssid;
    String password;
    Preferences preferences;
    Ticker reconnectTicker;
    unsigned long connectionAttempts;
    bool autoReconnect;
    int maxConnectionAttempts;

    // Variáveis para controle do scan assíncrono
    bool scanInProgress;
    unsigned long scanStartTime;
    static constexpr unsigned long SCAN_TIMEOUT = 10000; // 10 segundos de timeout

    void loadCredentials()
    {
        if (!preferences.begin("wifi-config", true))
        {
            Serial.println("Erro ao abrir preferences para leitura");
            return;
        }

        ssid = preferences.getString("ssid", "");
        password = preferences.getString("password", "");

        Serial.printf("Credenciais carregadas - SSID: %s\n", ssid.c_str());
        preferences.end();
    }

    void saveCredentials()
    {
        if (!preferences.begin("wifi-config", false))
        {
            Serial.println("Erro ao abrir preferences para escrita");
            return;
        }

        preferences.putString("ssid", ssid);
        preferences.putString("password", password);

        Serial.println("Credenciais WiFi salvas na flash");
        preferences.end();
    }

    void startAP()
    {
        const char *apSSID = "ESP32-Config";
        const char *apPassword = "config1234"; // AP protegido

        if (!WiFi.softAP(apSSID, apPassword))
        {
            Serial.println("Falha ao iniciar modo AP");
            return;
        }

        Serial.println("Modo AP Iniciado");
        Serial.printf("SSID: %s\n", apSSID);
        Serial.print("IP: ");
        Serial.println(WiFi.softAPIP());

        // Inicia DNS server para captive portal
        if (dns)
        {
            dns->start(53, "*", WiFi.softAPIP());
            Serial.println("DNS Server iniciado para captive portal");
        }

        isInConfigurationMode = true;
    }

    void stopAP()
    {
        if (dns)
        {
            dns->stop();
        }
        WiFi.softAPdisconnect(true);
        isInConfigurationMode = false;
        Serial.println("Modo AP desativado");
    }

    void setupWebServer()
    {
        // Página de configuração principal
        server->on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
            String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Configuração WiFi</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 500px; margin: 0 auto; }
        .card { background: #f9f9f9; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { background: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }
        .status { padding: 10px; border-radius: 4px; margin-bottom: 20px; }
        .connected { background: #d4edda; color: #155724; }
        .disconnected { background: #f8d7da; color: #721c24; }
        .ap-mode { background: #fff3cd; color: #856404; }
        #networksList { margin-top: 15px; }
        .network-item { padding: 8px; border-bottom: 1px solid #eee; cursor: pointer; }
        .network-item:hover { background: #f0f0f0; }
        .signal-bar { width: 60px; height: 10px; background: #ddd; margin-right: 8px; display: inline-block; }
        .signal-strength { height: 100%; }
        .excellent { background: #4CAF50; }
        .good { background: #8BC34A; }
        .fair { background: #FFC107; }
        .weak { background: #FF9800; }
        .poor { background: #F44336; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Configuração WiFi</h1>
        <div class="card">
)=====";

            if (WiFi.isConnected()) {
                html += R"=====(
            <div class="status connected">
                Conectado a: )=====" + ssid + R"=====(<br>
                IP: )=====" + WiFi.localIP().toString() + R"=====(
            </div>
)=====";
            } else if (isInConfigurationMode) {
                html += R"=====(
            <div class="status ap-mode">
                Modo de configuração ativo<br>
                Conecte-se a rede: ESP32-Config<br>
                IP: )=====" + WiFi.softAPIP().toString() + R"=====(
            </div>
)=====";
            } else {
                html += R"=====(
            <div class="status disconnected">
                Não conectado a nenhuma rede WiFi
            </div>
)=====";
            }

            html += R"=====(
            <form method="post" action="/wifi/save">
                <div class="form-group">
                    <label for="ssid">Rede WiFi (SSID)</label>
                    <input type="text" id="ssid" name="ssid" value=")=====" + ssid + R"=====(" required>
                    <button type="button" onclick="scanNetworks()" style="margin-top: 8px;">Buscar Redes</button>
                    <div id="networksList"></div>
                </div>
                <div class="form-group">
                    <label for="password">Senha</label>
                    <input type="password" id="password" name="password" value=")=====" + password + R"=====(">
                </div>
                <button type="submit">Salvar e Conectar</button>
            </form>
        </div>
    </div>
    <script>
    function scanNetworks() {
        const listDiv = document.getElementById('networksList');
        listDiv.innerHTML = '<p style="color:#555;">Buscando redes WiFi...</p>';
        
        fetch('/wifi/scan')
            .then(response => {
                if (!response.ok) throw new Error('Erro na requisição');
                return response.json();
            })
            .then(data => {
                if (data.status === 'scan_started') {
                    checkScanResults();
                } else {
                    listDiv.innerHTML = '<p style="color:red;">Erro: ' + (data.error || 'Falha ao iniciar scan') + '</p>';
                }
            })
            .catch(error => {
                listDiv.innerHTML = '<p style="color:red;">Erro: ' + error.message + '</p>';
            });
    }

    function checkScanResults() {
        fetch('/wifi/scan/results')
            .then(response => {
                if (!response.ok) throw new Error('Erro na requisição');
                return response.json();
            })
            .then(data => {
                if (data.status === 'scan_in_progress') {
                    setTimeout(checkScanResults, 1000);
                } else if (Array.isArray(data)) {
                    updateNetworkList(data);
                } else if (data.error) {
                    document.getElementById('networksList').innerHTML = 
                        '<p style="color:red;">Erro: ' + data.error + '</p>';
                }
            })
            .catch(error => {
                document.getElementById('networksList').innerHTML = 
                    '<p style="color:red;">Erro: ' + error.message + '</p>';
            });
    }

    function updateNetworkList(networks) {
        const listDiv = document.getElementById('networksList');
        if (networks.length === 0) {
            listDiv.innerHTML = '<p style="color:#555;">Nenhuma rede encontrada</p>';
            return;
        }

        let html = '<div style="max-height: 200px; overflow-y: auto;">';
        networks.forEach(network => {
            const signalStrength = Math.min(Math.max(2 * (network.rssi + 100), 0), 100);
            let signalClass;
            if (signalStrength > 80) signalClass = 'excellent';
            else if (signalStrength > 60) signalClass = 'good';
            else if (signalStrength > 40) signalClass = 'fair';
            else if (signalStrength > 20) signalClass = 'weak';
            else signalClass = 'poor';

            html += `
                <div class="network-item" onclick="selectNetwork('${escapeHtml(network.ssid)}', ${network.secure})">
                    <strong>${escapeHtml(network.ssid)}</strong>
                    <div style="display: flex; align-items: center; margin-top: 4px;">
                        <div class="signal-bar">
                            <div class="signal-strength ${signalClass}" style="width: ${signalStrength}%"></div>
                        </div>
                        ${network.secure ? '🔒' : '🌐'}
                        <span style="margin-left: 8px; font-size: 0.9em; color: #666;">${network.rssi} dBm</span>
                    </div>
                </div>`;
        });
        html += '</div>';
        
        listDiv.innerHTML = html;
    }

    function selectNetwork(ssid, isSecure) {
        document.getElementById('ssid').value = ssid;
        if (isSecure) {
            document.getElementById('password').focus();
        }
    }

    function escapeHtml(unsafe) {
        return unsafe
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#039;");
    }
    </script>
</body>
</html>
)=====";

            request->send(200, "text/html", html); });

        // Endpoint para salvar configurações
        server->on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest *request)
                   {
            if (request->hasParam("ssid", true)) {
                ssid = request->getParam("ssid", true)->value();
            }
            if (request->hasParam("password", true)) {
                password = request->getParam("password", true)->value();
            }

            saveCredentials();
            
            String html = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="refresh" content="5;url=/wifi">
    <title>Configuração Salva</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .message { background: #d4edda; color: #155724; padding: 15px; border-radius: 4px; }
    </style>
</head>
<body>
    <div class="message">
        <h1>Configuração Salva!</h1>
        <p>Conectando à rede: )=====" + escapeHtml(ssid) + R"=====(</p>
        <p>O dispositivo irá reiniciar em 5 segundos...</p>
    </div>
</body>
</html>
)=====";

            request->send(200, "text/html", html);
            
            // Dar tempo para a resposta ser enviada antes de reiniciar
            delay(1000);
            ESP.restart(); });

        // Endpoint para iniciar scan de redes WiFi (assíncrono)
        server->on("/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
            if (scanInProgress) {
                request->send(202, "application/json", "{\"status\":\"scan_in_progress\"}");
                return;
            }

            // Inicia o scan assíncrono
            WiFi.scanDelete();
            int scanResult = WiFi.scanNetworks(true, true); // async, hidden
            
            if (scanResult == WIFI_SCAN_RUNNING) {
                scanInProgress = true;
                scanStartTime = millis();
                request->send(202, "application/json", "{\"status\":\"scan_started\"}");
            } else if (scanResult < 0) {
                request->send(500, "application/json", "{\"error\":\"scan_failed_to_start\",\"code\":" + String(scanResult) + "}");
            } });

        // Endpoint para verificar resultados do scan
        server->on("/wifi/scan/results", HTTP_GET, [this](AsyncWebServerRequest *request)
                   {
            if (!scanInProgress) {
                request->send(200, "application/json", "{\"status\":\"no_scan_in_progress\"}");
                return;
            }

            int scanStatus = WiFi.scanComplete();
            
            if (scanStatus == WIFI_SCAN_RUNNING) {
                // Verifica se o timeout foi atingido
                if (millis() - scanStartTime > SCAN_TIMEOUT) {
                    WiFi.scanDelete();
                    scanInProgress = false;
                    request->send(408, "application/json", "{\"error\":\"scan_timeout\"}");
                } else {
                    request->send(202, "application/json", "{\"status\":\"scan_in_progress\"}");
                }
                return;
            }

            // Scan completo
            scanInProgress = false;
            
            if (scanStatus < 0) {
                request->send(500, "application/json", "{\"error\":\"scan_failed\",\"code\":" + String(scanStatus) + "}");
                return;
            }

            // Processa os resultados
            String json = "[";
            for (int i = 0; i < scanStatus; ++i) {
                if (i) json += ",";
                json += "{\"ssid\":\"" + escapeJson(WiFi.SSID(i)) + "\",\"rssi\":" + WiFi.RSSI(i) + 
                       ",\"secure\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
            }
            json += "]";
            
            request->send(200, "application/json", json);
            
            // Limpa os resultados após enviar
            WiFi.scanDelete(); });

        // Endpoint para resetar configurações
        server->on("/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest *request)
                   {
            resetSettings();
            request->send(200, "application/json", "{\"status\":\"settings_reset\"}");
            delay(1000);
            ESP.restart(); });
    }

    String escapeJson(const String &input)
    {
        String output;
        output.reserve(input.length() * 2); // Reserva espaço para caracteres escapados

        for (size_t i = 0; i < input.length(); ++i)
        {
            char c = input.charAt(i);
            switch (c)
            {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (c <= '\x1f')
                {
                    // Caracteres de controle (0x00-0x1f)
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    output += buf;
                }
                else
                {
                    output += c;
                }
            }
        }

        return output;
    }

    String escapeHtml(const String &input)
    {
        String output;
        output.reserve(input.length() * 2);

        for (size_t i = 0; i < input.length(); ++i)
        {
            char c = input.charAt(i);
            switch (c)
            {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '"':
                output += "&quot;";
                break;
            case '\'':
                output += "&#039;";
                break;
            default:
                output += c;
            }
        }

        return output;
    }

    void tryConnect()
    {
        if (ssid.length() == 0)
        {
            Serial.println("Nenhuma rede WiFi configurada");
            startAP();
            return;
        }

        Serial.printf("Tentando conectar a: %s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());

        connectionAttempts = 0;
        autoReconnect = true;
    }

    void checkConnection()
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            if (isInConfigurationMode)
            {
                Serial.println("Conexão WiFi estabelecida, desativando modo AP");
                stopAP();
            }
            return;
        }

        if (autoReconnect && connectionAttempts < maxConnectionAttempts)
        {
            connectionAttempts++;
            Serial.printf("Tentativa de conexão %d/%d\n", connectionAttempts, maxConnectionAttempts);

            if (connectionAttempts >= maxConnectionAttempts)
            {
                Serial.println("Máximo de tentativas alcançado, iniciando modo AP");
                startAP();
            }
        }
    }

public:
    WiFiManager(AsyncWebServer *existingServer = nullptr, DNSServer *dnsServer = nullptr, int maxAttempts = 10)
        : server(existingServer),
          dns(dnsServer),
          isInConfigurationMode(false),
          autoReconnect(false),
          maxConnectionAttempts(maxAttempts),
          connectionAttempts(0),
          scanInProgress(false),
          scanStartTime(0)
    {
        if (!server)
        {
            // Se nenhum servidor foi fornecido, cria um novo
            server = new AsyncWebServer(80);
            Serial.println("Criado novo servidor web na porta 80");
        }

        loadCredentials();
    }

    ~WiFiManager()
    {
        if (reconnectTicker.active())
        {
            reconnectTicker.detach();
        }
    }

    void begin()
    {
        autoConnect();
    }

    void autoConnect()
    {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);

        tryConnect();
        setupWebServer();
        server->begin();

        // Verificar conexão a cada 5 segundos
        // reconnectTicker.attach(5, [this]() { checkConnection(); });
    }

    void process()
    {
        // Método para compatibilidade com outras bibliotecas
        if (dns && isInConfigurationMode)
        {
            dns->processNextRequest();
        }
    }

    void resetSettings()
    {
        if (!preferences.begin("wifi-config", false))
        {
            Serial.println("Erro ao abrir preferences para reset");
            return;
        }

        preferences.clear();
        preferences.end();

        ssid = "";
        password = "";

        Serial.println("Configurações WiFi resetadas");
    }

    bool isInConfigMode() const
    {
        return isInConfigurationMode;
    }

    String getSSID() const
    {
        return ssid;
    }

    String getPassword() const
    {
        return password;
    }

    bool isConnected() const
    {
        return WiFi.status() == WL_CONNECTED;
    }

    IPAddress getLocalIP() const
    {
        return WiFi.localIP();
    }

    IPAddress getAPIP() const
    {
        return WiFi.softAPIP();
    }
};

#endif // LOCAL_WIFI_MANAGER_H