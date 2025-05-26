#include "LoRaCom.h"
#include "logger.h"

#ifdef TTGO
#include "LoRaTTGO.h"

#elif RF95
#include <LoRaRF95.h>
#include "LoRaRF95.h"

#endif

#include "config.h"
#include "system_state.h"
#include "display_manager.h"
#include "device_info.h"

// Define the static member
LoRaInterface *LoRaCom::loraInstance = nullptr;

// Define the static receiveCallback pointer
void (*LoRaCom::onReceiveCallback)(LoRaInterface *) = nullptr;

// ========== LoRaCom Implementations ==========
bool LoRaCom::initialize()
{
#ifdef TTGO
    setInstance(new LoRaTTGO());
#elif RF95
    setInstance(new LoRaRF95());
#endif

#ifdef TTGO
    loraInstance->setPins(Config::LORA_CS_PIN, Config::LORA_RESET_PIN, Config::LORA_IRQ_PIN);
#endif
    bool rt = loraInstance->beginSetup(Config::LORA_BAND,
#ifdef TTGO
                                       false
#else
                                       true
#endif
    );
    if (!rt)
    {
        Logger::log(LogLevel::ERROR, F("Falha ao iniciar LoRa"));
        return false;
    }
    loraInstance->setHeaderFrom(Config::TERMINAL_ID);
    displayManager.setVersion(Config::LMCU_VER);

    loraInstance->endSetup();
    Logger::log(LogLevel::INFO, F("LoRa inicializado com sucesso"));
    systemState.setLoraStatus(true);
    return true;
}

long lastSendTime = -20000;
void LoRaCom::handle()
{
    if (loraInstance->available())
    {
        if (onReceiveCallback)
        {
            onReceiveCallback(loraInstance);
        }
        else
        {
            Logger::log(LogLevel::ERROR, F("Não definiu callback para receber os dados"));
        }
    }
    else
    {
#ifdef TTGO
        static uint32_t ultimoRssi = 0;
        if (millis() - ultimoRssi > 1000)
        {
            displayManager.rssi = loraInstance->packetRssi();
            displayManager.snr = loraInstance->packetSnr();
            ultimoRssi = millis();
        }
#endif
    }

    if (millis() - lastSendTime > 30000)
    {
        lastSendTime = millis();
        LoRaCom::sendTime();
    }
}

void LoRaCom::formatMessage(char *message, uint8_t tid, const char *event, const char *value)
{

    memset(message, 0, sizeof(message));
    // snprintf(message,sizeof(message), "{\"dtype\":\"%s\",\"event\":\"%s\",\"value\":\"%s\"}", name, event, value);
    sprintf(message, "%s|%s", event, value);
}

void LoRaCom::ack(bool ak, uint8_t tid)
{
    loraInstance->setHeaderTo(tid);
    bool rt = loraInstance->print((ak) ? "ack" : "nak");
    displayManager.message((String)tid + "->" + (ak) ? "ack" : "nak");
}

void LoRaCom::sendHeaderTo(uint8_t tid)
{
    loraInstance->setHeaderTo(tid);
}

int LoRaCom::packetRssi()
{

    int8_t rssi = loraInstance->packetRssi();
    return rssi;
}

void LoRaCom::sendTime()
{
    sendCommand("time", DeviceInfo::getISOTime(), 0xFF);
}

void LoRaCom::sendPresentation(const uint8_t tid, const uint8_t n)
{
#ifdef DEBUG_ON
    Serial.print("enter presentation");
#endif
    char message[Config::MESSAGE_LEN];
    char nStr[8];
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {

        // snprintf(nStr, sizeof(nStr), "%u", attempt + 1);
        formatMessage(message, tid, "presentation", "ttgo");
        displayManager.message(message);
        loraInstance->setHeaderTo(tid);
        bool rt = loraInstance->print(message);
        if (rt)
        { // não pega ack se for broadcast por timeout  n == 1
            Logger::log(LogLevel::ERROR, F("Falha ao enviar apresentação LoRa"));
            return;
        }

        Logger::info("Presentation");
        Logger::log(LogLevel::DEBUG, String((String)message + " (tentativa " + String(attempt + 1) + ")").c_str());
    }
#ifdef DEBUG_ON
    Serial.print("saiu prensentation");
#endif
}

bool LoRaCom::waitAck()
{
#ifdef DEBUG_ON
    Serial.print("entrou waitAck()");
#endif
    // Wait for acknowledgment
    unsigned long start = millis();
    unsigned long lastCheck = millis();
    while (millis() - start < Config::ACK_TIMEOUT_MS)
    {
        if (millis() - lastCheck >= 100)
        {
            lastCheck = millis();
            // Verifica se há pacote disponível
        }
        digitalWrite(LED_BUILTIN, HIGH); // Indicate waiting for ACK
                                         // delay(100);

        uint8_t tid = 0xFF;
        char payload[Config::MESSAGE_LEN];
        uint16_t index = 0;
        int n = 0;
        while (loraInstance->available() && index < sizeof(payload) - 1)
        {
            uint8_t byte = loraInstance->read();
            n++;
            if ((n == 1) || !(byte >= 32 && byte <= 126))
                tid = byte;
            if (n < 5)
                continue;
            payload[index] = static_cast<char>(byte);
            index++;
            loraInstance->setHeaderTo(tid);
            if (static_cast<char>(byte) == '}')
                break;
        }
        payload[index] = '\0'; // Garante terminação

        systemState.loraRcv(payload);

        Logger::log(LogLevel::INFO, payload);
#ifdef DEBUG_ON
        Serial.print("saiu waitAck()");
#endif
        return true;
    }

    digitalWrite(LED_BUILTIN, LOW); // Turn off LED

    // ack(false);
    return false;
}

bool LoRaCom::sendCommand(const String event, const String value, uint8_t tid)
{
#ifdef DEBUG_ON
    Serial.print("entrou sendCommand");
#endif
    char output[Config::MESSAGE_LEN];
    memset(output, 0, sizeof(output));
    formatMessage(output, tid, event.c_str(), value.c_str());
    // Garante null-terminator
    output[Config::MESSAGE_LEN - 1] = '\0';
    size_t msgLen = strlen(output);
    if (msgLen == 0 || output[msgLen - 1] != '\0')
    {
        output[msgLen] = '\0';
    }
    loraInstance->setHeaderTo(tid);
    bool rt = loraInstance->sendMessage(tid, output);

#ifdef DEBUG_ON
    Serial.print("saindo sendMessage");
#endif
    if (!rt)
    {
        Logger::log(LogLevel::ERROR, F("Falha ao enviar comando LoRa"));
        return false;
    }
    displayManager.message((String)(tid) + "->" + event + ": " + value);
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

void LoRaCom::setReceiveCallback(void (*callback)(LoRaInterface *))
{
    onReceiveCallback = callback;
}
