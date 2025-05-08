#include "transmissor.h"
#include "logger.h"

// ========== Instâncias Globais ==========
Adafruit_SSD1306 display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1);
TuyaWifi my_device;
WebServer server(Config::WEBSERVER_PORT);
SystemState systemState;

// ========== Implementações ==========

// ========== SystemState Implementations ==========
void SystemState::updateState(const String &newState)
{
    if (relayState != newState)
    {
        relayState = (newState == "on" ? "LIGADO" : "DESLIGADO");
        lastUpdate = millis();
        // Logger::log(LogLevel::INFO, "Estado atualizado para: " + newState);
        updateDisplay();
    }
}

void SystemState::setWifiStatus(bool connected)
{
    wifiConnected = connected;
    updateDisplay();
}

void SystemState::setLoraStatus(bool initialized)
{
    loraInitialized = initialized;
    updateDisplay();
}

void SystemState::setLoraEvent(const String &event, const String &value)
{
    loraRcvEvent = event;
    loraRcvValue = value;
    updateDisplay();
}

void SystemState::resetDisplayUpdate()
{
    lastDisplayUpdate = millis();
}

String SystemState::getState() const { return relayState; }
bool SystemState::isWifiConnected() const { return wifiConnected; }
bool SystemState::isLoraInitialized() const { return loraInitialized; }

bool SystemState::isStateValid() const
{
    return (millis() - lastUpdate) < Config::STATE_TIMEOUT_MS;
}

String SystemState::getISOTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Logger::log(LogLevel::WARNING, "Falha ao obter horário NTP");
        return "1970-01-01T00:00:00Z";
    }

    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(timeStr);
}

void SystemState::loraRcv(const String &message)
{
    if (message.length() < sizeof(loraRcvMessage))
    {
        strncpy(loraRcvMessage, message.c_str(), sizeof(loraRcvMessage) - 1);
        loraRcvMessage[sizeof(loraRcvMessage) - 1] = '\0'; // Garante terminação
    }
    Logger::log(LogLevel::DEBUG, String("RCV: " + message).c_str());
}

static long ultimoBaixo = 0;
void SystemState::updateDisplay()
{
    display.clearDisplay();
    display.setCursor(0, 0);

    display.print("WiFi: ");
    display.println(wifiConnected ? WiFi.localIP().toString() : "DESCONECTADO");

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    bool baixo = (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD);
    if (baixo && (millis() - ultimoBaixo > 30000))
    {
        Logger::log(LogLevel::WARNING, String("Sinal LoRa baixo: RSSI: " + String(rssi) + " SNR: " + String(snr)).c_str());
        ultimoBaixo = millis();
    }

    display.print("Radio: ");
    display.print(loraInitialized ? (baixo) ? "Baixo" : "OK" : "FALHA");
    display.print(" (");
    display.print(rssi);
    display.println(")");

    display.print("Estado: ");
    display.println(relayState);

    display.print("Atualizado: ");
    display.println(getISOTime().substring(11, 19)); // Mostra apenas HH:MM:SS

    if (loraRcvEvent.length() > 0)
    {
        display.print("Evento: ");
        display.println(loraRcvEvent);
        display.print("Value: ");
        display.println(loraRcvValue);
    }
    else
    {
        display.print("Evento: NENHUM");
    }

    display.display();
}

void SystemState::conditionalUpdateDisplay()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastDisplayUpdate >= Config::DISPLAY_UPDATE_INTERVAL)
    {
        updateDisplay();
        lastDisplayUpdate = currentMillis;
    }
}

// Função chamada quando um pacote LoRa é recebido
void onReceiveCallback(int packetSize)
{
    Logger::verbose("onReceiveCallback");
    // if (packetSize == 0)
    //     return; // Se não há pacote, ignora

    // Serial.print("Pacote recebido (" + String(packetSize) + " bytes): ");

    LoRaCom::processIncoming();

    /*
        // Lê os dados do pacote
        while (LoRa.available())
        {
            String data = LoRa.readString();
            Serial.println(data);
        }
    */
    // Exibe RSSI (força do sinal)
    // Serial.println("RSSI: " + String(LoRa.packetRssi()) + " dBm");
}
// ========== LoRaCom Implementations ==========
bool LoRaCom::initialize()
{
    LoRa.setPins(Config::LORA_CS_PIN, Config::LORA_RESET_PIN, Config::LORA_IRQ_PIN);

    LoRa.onReceive(onReceiveCallback);
    if (!LoRa.begin(Config::LORA_BAND))
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar LoRa");
        return false;
    }

    LoRa.setSyncWord(Config::LORA_SYNC_WORD);
    // LoRa.setTxPower(20); // Potência máxima
    LoRa.setPreambleLength(8);
    LoRa.setCodingRate4(4);
    // LoRa.setSpreadingFactor(7);     // SF7 (mais rápido)
    // LoRa.setSignalBandwidth(500e3); // BW = 500 kHz (maior largura = mais velocidade)
    // LoRa.setCodingRate4(4);         // CR = 4/5 (sem redundância extra)
    LoRa.setTxPower(5); // TX Power baixo (5 dBm)
    // LoRa.enableCrc();               // CRC ativado para segurança

    Logger::log(LogLevel::INFO, "LoRa inicializado com sucesso");
    systemState.setLoraStatus(true);
    return true;
}

void LoRaCom::formatMessage(char *message, uint8_t tid, const char *event, const char *value)
{
    sprintf(message, "{\"dtype\":\"gateway\",\"event\":\"%s\",\"value\":\"%s\"}\0", event, value);
}

void LoRaCom::ack(bool ak, uint8_t tid)
{
    char ackBuffer[Config::MESSAGE_LEN];

    formatMessage(ackBuffer, tid, (ak) ? "ack" : "nak", "");

    LoRa.beginPacket();
    String ackMessage = (String)ackBuffer;
    sendHeaderTo(tid);
    LoRa.print((ak) ? "ack" : "nak");
    if (LoRa.endPacket() == 0)
    {
        Logger::log(LogLevel::ERROR, "Falha ao enviar ACK/NACK");
    }
    else
    {
        // Logger::log(LogLevel::INFO, "ACK/NACK enviado: " + ackMessage);
    }
}

uint8_t nHeaderId = 0;
uint8_t LoRaCom::genHeaderId()
{
    if (nHeaderId >= 255)
        nHeaderId = 0;
    return nHeaderId++;
}

void LoRaCom::sendHeaderTo(uint8_t tid)
{
    char msg[4];
    msg[0] = tid;
    msg[1] = Config::TERMINAL_ID;
    msg[2] = LoRaCom::genHeaderId();
    msg[3] = 0xFF;
    LoRa.print(msg);
}
void LoRaCom::sendPresentation(uint8_t tid, uint8_t n)
{
    char message[Config::MESSAGE_LEN];
    char nStr[8];
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {

        snprintf(nStr, sizeof(nStr), "%u", attempt + 1);
        formatMessage(message, tid, "presentation", nStr);

        LoRa.beginPacket();
        // Serial.println(message);
        sendHeaderTo(tid);
        LoRa.print(message);
        if (LoRa.endPacket() == 0)
        { // não pega ack se for broadcast por timeout  n == 1
            Logger::log(LogLevel::ERROR, "Falha ao enviar apresentação LoRa");
            return;
        }
        Logger::info("Presentation");
        Logger::log(LogLevel::DEBUG, String((String)message + " (tentativa " + String(attempt + 1) + ")").c_str());
        /*   ackReceived = waitAck();
           if (!ackReceived)
           {
               Logger::log(LogLevel::WARNING, "Tentando reenviar apresentação...");
           } */
    }
    /*
    if (!ackReceived)
    {
        Logger::log(LogLevel::ERROR, "Falha ao receber ACK para apresentação LoRa");
    }
    else
    {
        Logger::log(LogLevel::INFO, "ACK recebido para apresentação LoRa");
    }*/
}

bool LoRaCom::waitAck()
{
    // Wait for acknowledgment
    unsigned long start = millis();
    unsigned long lastCheck = millis();
    uint8_t tid = 0xFF;
    while (millis() - start < Config::ACK_TIMEOUT_MS)
    {
        if (millis() - lastCheck >= 100)
        {
            lastCheck = millis();
            // Verifica se há pacote disponível
        }
        digitalWrite(BUILTIN_LED, HIGH); // Indicate waiting for ACK
        // delay(100);
        if (LoRa.parsePacket())
        {
            char payload[Config::MESSAGE_LEN];
            int index = 0;
            int n = 0;
            while (LoRa.available() && index < sizeof(payload) - 1)
            {
                uint8_t byte = LoRa.read();
                n++;
                if ((n == 1) || !(byte >= 32 && byte <= 126))
                    tid = byte;
                if (n < 5)
                    continue;
                payload[index] = static_cast<char>(byte);
                index++;
                if (static_cast<char>(byte) == '}')
                    break;
            }
            payload[index] = '\0'; // Garante terminação

            systemState.loraRcv(payload);

            Logger::log(LogLevel::INFO, payload);
            return true;
        }
        else
        {
            digitalWrite(BUILTIN_LED, LOW);
            // delay(100);
        }

        digitalWrite(BUILTIN_LED, LOW); // Turn off LED
    }
    // Serial.println("ACK not received within timeout.");
    ack(false);
    return false;
}

bool LoRaCom::sendCommand(const String event, const String value, uint8_t tid)
{
    char output[Config::MESSAGE_LEN];
    formatMessage(output, tid, event.c_str(), value.c_str());
    // Serial.println(output);

    LoRa.beginPacket();

    sendHeaderTo(tid);

    LoRa.print(output);
    if (LoRa.endPacket() == 0)
    {
        Logger::log(LogLevel::ERROR, "Falha ao enviar comando LoRa");
        return false;
    }
    Logger::info("Enviou");
    Logger::log(LogLevel::DEBUG, String(output).c_str());

    // Feedback visual
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Comando enviado:");
    display.println(event);
    display.display();
    // Aguarda ~1 segundo sem bloquear, permitindo background
    // sleep(100);
    return true;
}

void LoRaCom::sleep(unsigned int duration)
{
    unsigned long start = millis();
    while (millis() - start < duration)
    {
        yield(); // Permite que outras tarefas sejam executadas
        delay(10);
    }
}

void LoRaCom::processIncoming()
{

    if (LoRa.parsePacket())
    {
        String payload = "";
        uint8_t tid = 0xFF; // no future, ler no pacote [1]
        uint8_t pos = 0;
        while (LoRa.available())
        {
            uint8_t byte = LoRa.read();
            char cbyte = static_cast<char>(byte);
            // Serial.print(" ");
            // Serial.print(byte, HEX);
            // Serial.print(":");
            // Serial.print(cbyte);

            if (pos++ == 1)
                tid = byte;

            if (pos < 5 || (byte == 0xFF) || !(byte >= 32 && byte <= 126)) // Ignora bytes de controle
                continue;
            payload += cbyte;
            if (cbyte == '}')
                break;
        }
        // Serial.println("");
        if (payload.length() == 0)
        {
            Logger::log(LogLevel::WARNING, "Payload LoRa vazio");
            // LoRa.idle();
            return;
        }

        systemState.loraRcv(payload);

        int rssi = LoRa.packetRssi();
        float snr = LoRa.packetSnr();
        // Logger::log(LogLevel::INFO, "RSSI: " + String(rssi) + ", SNR: " + String(snr));

        if (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD)
        {
            Logger::log(LogLevel::WARNING, "Qualidade do link baixa. Tentando ajustar...");

            // mostrar no display
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("Qualidade LoRa baixa!");
            display.print("RSSI: ");
            display.println(rssi);
            display.print("SNR: ");
            display.println(snr, 1);
            display.display();

            // Ajuste parâmetros do rádio, se necessário
        }
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, payload.c_str()))
        {
            Logger::log(LogLevel::WARNING, "Payload LoRa inválido");
            LoRaCom::ack(false);
            return;
        }

        // Compatibilidade com ambos os formatos
        const char *event = doc["event"]; // Formato do receptor
        const char *value = doc["value"]; // Formato do receptor
                                          // const uint8_t sid = doc["sid"];   // Formato do receptor

        systemState.setLoraEvent(event, value); // Atualiza o estado do sistema

        // LoRaCom::ack(true, sid);

        if (event == nullptr)
        {
            event = doc["state"]; // Formato alternativo
        }

        if (event != nullptr)
        {
            if (String(event) == "status")
            {
                // Apenas atualiza o estado sem acionar comandos extras
                systemState.updateState(String(value));

                // Atualiza Tuya Cloud
                unsigned char dp_id = 1;
                unsigned char dp_value = (systemState.getState() == "LIGADO") ? 1 : 0;
                // my_device.mcu_dp_update(dp_id, &dp_value, sizeof(dp_value));
                ack(true, tid);
            }
            if (String(event) == "presentation")
            {
                // Envia apresentação para o dispositivo
                // registra o dispostitvo na whitelist
                LoRaCom::ack(true, 0xFF);
            }
            else if (String(event) == "ack")
            {
                // Processa ACK
                // Logger::log(LogLevel::INFO, "ACK recebido: " + String(value));
            }
            else if (String(event) == "nak")
            {
                // Processa NACK
                // Logger::log(LogLevel::ERROR, "NACK recebido: " + String(value));
            }
            else
            {
                // systemState.updateState((String)event + "=" + value);
            }
        }
        else
        {
            //  systemState.updateState((String)event + "=" + value);
        }
    }
    else
    {
    }
}

// ========== Tuya Callback Implementation ==========
unsigned char handleTuyaCommand(unsigned char dp_id, const unsigned char dp_data[], unsigned short dp_len)
{
    switch (dp_data[0])
    {
    case 0:
        LoRaCom::sendCommand("desligar", "OFF", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: DESLIGAR");
        break;
    case 1:
        LoRaCom::sendCommand("ligar", "OK", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: LIGAR");
        break;
    case 2:
        LoRaCom::sendCommand("revert", "REVERTER", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: REVERTER");
        break;
    case 3:
        LoRaCom::sendCommand("status", "", dp_id);
        Logger::log(LogLevel::INFO, "Comando Tuya: STATUS");
        break;
    default:
        //  Logger::log(LogLevel::WARNING, "Comando Tuya desconhecido: " + String(dp_data[0]));
        return 0;
    }
    systemState.resetDisplayUpdate();
    return 1;
}

// ========== Network Functions Implementations ==========
void initWiFi()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initWiFi");
    WiFiManager wifiManager;
    wifiManager.setTimeout(Config::WIFI_TIMEOUT_S);

    if (!wifiManager.autoConnect(Config::WIFI_AP_NAME))
    {
        Logger::log(LogLevel::ERROR, "Falha ao conectar WiFi");
        ESP.restart();
    }

    systemState.setWifiStatus(true);
    // Logger::log(LogLevel::INFO, "WiFi conectado: " + WiFi.localIP().toString());
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWiFi com sucesso");
}

void initNTP()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initNTP");
    configTime(Config::GMT_OFFSET_SEC, Config::DAYLIGHT_OFFSET_SEC, Config::NTP_SERVER);
    Logger::log(LogLevel::INFO, "Sincronizando com NTP...");
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        Logger::log(LogLevel::WARNING, "Falha ao sincronizar com NTP. Tentando novamente em 1 minuto...");
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com falha");
    }
    else
    {
        Logger::log(LogLevel::INFO, "Sincronização com NTP bem-sucedida.");
        // Logger::log(LogLevel::VERBOSE, "Horário sincronizado: " + String(asctime(&timeinfo)));
        Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initNTP com sucesso");
    }
}

// ========== Web Server Implementations ==========
String generateHtmlPage()
{
    String html = "<!DOCTYPE html><html lang='pt-BR'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Controle TTGO</title>";
    html += "<style>";
    html += "  * { box-sizing: border-box; }";
    html += "  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }";
    html += "  .container { max-width: 800px; margin: 0 auto; }";
    html += "  header { text-align: center; margin-bottom: 30px; }";
    html += "  h1 { color: #2c3e50; margin-bottom: 10px; }";
    html += "  .card { background: white; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); padding: 25px; margin-bottom: 25px; }";
    html += "  h2 { color: #3498db; margin-top: 0; border-bottom: 1px solid #eee; padding-bottom: 10px; }";
    html += "  .button-group { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 15px; }";
    html += "  button { background: #3498db; color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-size: 16px; transition: all 0.3s; flex: 1 1 120px; }";
    html += "  button:hover { background: #2980b9; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
    html += "  button:active { transform: translateY(0); }";
    html += "  #stateInfo { background: #f8f9fa; padding: 15px; border-radius: 6px; margin: 15px 0; font-weight: bold; border-left: 4px solid #3498db; }";
    html += "  .status { color: #7f8c8d; font-size: 14px; margin-top: 5px; }";
    html += "  .btn-danger { background: #e74c3c; }";
    html += "  .btn-danger:hover { background: #c0392b; }";
    html += "  .btn-success { background: #2ecc71; }";
    html += "  .btn-success:hover { background: #27ae60; }";
    html += "  .btn-warning { background: #f39c12; }";
    html += "  .btn-warning:hover { background: #d35400; }";
    html += "  @media (max-width: 600px) {";
    html += "    .button-group { flex-direction: column; }";
    html += "    button { width: 100%; }";
    html += "  }";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<div class='container'>";
    html += "  <header>";
    html += "    <h1>Controle TTGO</h1>";
    html += "    <p>Sistema de controle remoto</p>";
    html += "  </header>";

    html += "  <div class='card'>";
    html += "    <h2>Controle do Relé</h2>";
    html += "    <div class='button-group'>";
    html += "      <button onclick='getState()' class='btn-info'>Atualizar Estado</button>";
    html += "      <button onclick='turnOnRelay()' class='btn-success'>Ligar Relé</button>";
    html += "      <button onclick='turnOffRelay()' class='btn-danger'>Desligar Relé</button>";
    html += "      <button onclick='revertRelay()' class='btn-warning'>Inverter Relé</button>";
    html += "    </div>";
    html += "    <div id='stateInfo'>Estado atual: " + systemState.getState() + "</div>";
    html += "    <div class='status'>Última atualização: " + systemState.getISOTime() + "</div>";
    html += "  </div>";

    html += "  <div class='card'>";
    html += "    <h2>Gerenciamento do Sistema</h2>";
    html += "    <div class='button-group'>";
    html += "      <a href='/update'><button class='btn-info'>Atualização OTA</button></a>";
    html += "      <button onclick='location.reload()' class='btn-warning'>Recarregar Página</button>";
    html += "    </div>";
    html += "  </div>";
    html += "</div>";

    html += "<script>";
    html += "async function showAlert(message, isError = false) {";
    html += "  const alertBox = document.createElement('div');";
    html += "  alertBox.style.position = 'fixed';";
    html += "  alertBox.style.top = '20px';";
    html += "  alertBox.style.left = '50%';";
    html += "  alertBox.style.transform = 'translateX(-50%)';";
    html += "  alertBox.style.padding = '15px 25px';";
    html += "  alertBox.style.background = isError ? '#e74c3c' : '#2ecc71';";
    html += "  alertBox.style.color = 'white';";
    html += "  alertBox.style.borderRadius = '5px';";
    html += "  alertBox.style.boxShadow = '0 3px 10px rgba(0,0,0,0.2)';";
    html += "  alertBox.style.zIndex = '1000';";
    html += "  alertBox.textContent = message;";
    html += "  document.body.appendChild(alertBox);";
    html += "  setTimeout(() => alertBox.remove(), 3000);";
    html += "}";

    html += "async function fetchData(url, method = 'GET') {";
    html += "  try {";
    html += "    document.getElementById('stateInfo').textContent = 'Carregando...';";
    html += "    const response = await fetch(url, { method });";
    html += "    if (!response.ok) throw new Error('Erro na requisição');";
    html += "    return await response.text();";
    html += "  } catch (error) {";
    html += "    showAlert(error.message, true);";
    html += "    throw error;";
    html += "  }";
    html += "}";

    html += "async function getState() {";
    html += "  try {";
    html += "    const state = await fetchData('/getState');";
    html += "    document.getElementById('stateInfo').textContent = 'Estado atual: ' + state;";
    html += "  } catch {}";
    html += "}";

    html += "async function turnOnRelay() {";
    html += "  try {";
    html += "    const message = await fetchData('/turnOnRelay', 'POST');";
    html += "    showAlert('Relé ligado: ' + message);";
    html += "    await getState();";
    html += "  } catch {}";
    html += "}";

    html += "async function turnOffRelay() {";
    html += "  try {";
    html += "    const message = await fetchData('/turnOffRelay', 'POST');";
    html += "    showAlert('Relé desligado: ' + message);";
    html += "    await getState();";
    html += "  } catch {}";
    html += "}";

    html += "async function revertRelay() {";
    html += "  try {";
    html += "    const message = await fetchData('/revertRelay', 'POST');";
    html += "    showAlert('Relé invertido: ' + message);";
    html += "    await getState();";
    html += "  } catch {}";
    html += "}";
    html += "</script>";
    html += "</body></html>";

    return html;
}

void handleRootRequest()
{
    server.send(200, "text/html", generateHtmlPage());
}

void handleStateRequest()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleStateRequest");
    LoRaCom::sendCommand("get", "status", 0xFF);

    uint32_t start = millis();
    while (millis() - start < Config::COMMAND_TIMEOUT)
    {
        LoRaCom::processIncoming();
        if (systemState.getState() != "DESCONHECIDO")
        {
            break;
        }
        // delay(100);
    }

    server.send(200, "text/plain", systemState.getState());
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleStateRequest");
}

void handleRevertRelayRequest()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleRevertRelayRequest");
    LoRaCom::sendCommand("gpio", "revert", 0xFF);
    server.send(200, "text/plain", "Comando de reversão enviado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleRevertRelayRequest");
}

void handleTurnOnRelayRequest()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleTurnOnRelayRequest");
    LoRaCom::sendCommand("gpio", "on", 0xFF);
    server.send(200, "text/plain", "Comando para ligar o relé enviado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleTurnOnRelayRequest");
}

void handleTurnOffRelayRequest()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: handleTurnOffRelayRequest");
    LoRaCom::sendCommand("gpio", "off", 0xFF);
    server.send(200, "text/plain", "Comando para desligar o relé enviado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: handleTurnOffRelayRequest");
}

void initWebServer()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initWebServer");
    server.on("/", HTTP_GET, handleRootRequest);
    server.on("/getState", HTTP_GET, handleStateRequest);
    server.on("/revertRelay", HTTP_POST, handleRevertRelayRequest);
    server.on("/turnOnRelay", HTTP_POST, handleTurnOnRelayRequest);
    server.on("/turnOffRelay", HTTP_POST, handleTurnOffRelayRequest);

    ElegantOTA.begin(&server);
    server.begin();

    Logger::log(LogLevel::INFO, "Servidor HTTP iniciado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initWebServer");
}

// ========== Tuya Initialization ==========
void initTuya()
{
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: initTuya");
    my_device.init((unsigned char *)Config::LPID, (unsigned char *)Config::LMCU_VER);
    my_device.dp_process_func_register(handleTuyaCommand);
    Logger::log(LogLevel::INFO, "Tuya inicializado");
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: initTuya");
}

// ========== Main Setup and Loop ==========
void setup()
{
    Logger::setLogLevel(LogLevel::VERBOSE);
    Logger::log(LogLevel::VERBOSE, "Entrando no procedimento: setup");
    Serial.begin(Config::SERIAL_BAUD);
    Logger::log(LogLevel::INFO, "Iniciando sistema...");

    if (display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS))
    {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Iniciando...");
        display.display();
    }
    else
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar display OLED");
    }

    initWiFi();
    initNTP();

    if (!LoRaCom::initialize())
    {
        Logger::log(LogLevel::ERROR, "Falha crítica no LoRa - reiniciando");
        ESP.restart();
    }

    initTuya();
    initWebServer();

    systemState.updateDisplay();
    LoRa.receive();
    Logger::log(LogLevel::VERBOSE, "Saindo do procedimento: setup");
}

static uint32_t lastStateCheck = 0;
static uint32_t lastPresentationTime = 0; // Adiciona um controle para o envio de Presentation

void loop()
{
    //  LoRa.idle();
    // LoRaCom::processIncoming();
    my_device.uart_service();
    server.handleClient();

    systemState.conditionalUpdateDisplay();

    // Verifica o estado a cada intervalo definido
    if (millis() - lastStateCheck > Config::STATE_CHECK_INTERVAL)
    {
        if (!systemState.isStateValid())
        {
            LoRaCom::sendCommand("get", "status", 0xFF);
            systemState.resetDisplayUpdate();
        }
        lastStateCheck = millis();
    }

    // Envia mensagem de Presentation a cada 1 minuto
    if (millis() - lastPresentationTime > Config::PRESENTATION_INTERVAL) // 60000 ms = 1 minuto
    {
        LoRaCom::sendPresentation(0xFF, 1);
        lastPresentationTime = millis();
    }
    //   LoRa.receive();
}