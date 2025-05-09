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
#include <ArduinoJson.h>

// Define the static member
LoRaInterface *LoRaCom::loraInstance = nullptr;

// Função chamada quando um pacote LoRa é recebido
void onReceiveCallback(int packetSize)
{
    Logger::verbose("onReceiveCallback");
    LoRaCom::processIncoming();
}

// ========== LoRaCom Implementations ==========
bool LoRaCom::initialize()
{
#ifdef TTGO
    setInstance(new LoRaTTGO());
#elif RF95
    setInstance(new LoRaRF95());
#endif

    loraInstance->setPins(Config::LORA_CS_PIN, Config::LORA_RESET_PIN, Config::LORA_IRQ_PIN);
    loraInstance->onReceive(onReceiveCallback);
    bool rt = loraInstance->beginSetup(Config::LORA_BAND);
    if (!rt)
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar LoRa");
        return false;
    }
    loraInstance->setHeaderFrom(Config::TERMINAL_ID);

    loraInstance->endSetup();
    Logger::log(LogLevel::INFO, "LoRa inicializado com sucesso");
    systemState.setLoraStatus(true);
    return true;
}

void LoRaCom::handle()
{
    loraInstance->handle();
}

void LoRaCom::formatMessage(char *message, uint8_t tid, const char *event, const char *value)
{
    sprintf(message, "{\"dtype\":\"gateway\",\"event\":\"%s\",\"value\":\"%s\"}", event, value);
}

void LoRaCom::ack(bool ak, uint8_t tid)
{
    char ackBuffer[Config::MESSAGE_LEN];

    formatMessage(ackBuffer, tid, (ak) ? "ack" : "nak", "");

    String ackMessage = (String)ackBuffer;
    bool rt = loraInstance->print((ak) ? "ack" : "nak");
    if (rt)
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

#ifdef TTGO
    char msg[4];
    msg[0] = tid;
    msg[1] = Config::TERMINAL_ID;
    msg[2] = LoRaCom::genHeaderId();
    msg[3] = 0xFF;
    LoRa.print(msg);
#endif
}

int LoRaCom::packedRssi()
{

    int8_t rssi = loraInstance->packetRssi();
    return rssi;
}

void LoRaCom::sendPresentation(const uint8_t tid, const uint8_t n)
{
    char message[Config::MESSAGE_LEN];
    char nStr[8];
    bool ackReceived = false;
    for (int attempt = 0; attempt < n && !ackReceived; ++attempt)
    {

        snprintf(nStr, sizeof(nStr), "%u", attempt + 1);
        formatMessage(message, tid, "presentation", nStr);

        loraInstance->setHeaderTo(tid);
        bool rt = loraInstance->print(message);
        if (rt)
        { // não pega ack se for broadcast por timeout  n == 1
            Logger::log(LogLevel::ERROR, "Falha ao enviar apresentação LoRa");
            return;
        }

        Logger::info("Presentation");
        Logger::log(LogLevel::DEBUG, String((String)message + " (tentativa " + String(attempt + 1) + ")").c_str());
    }
}

bool LoRaCom::waitAck()
{
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
        return true;
    }

    digitalWrite(LED_BUILTIN, LOW); // Turn off LED

    ack(false);
    return false;
}

bool LoRaCom::sendCommand(const String event, const String value, uint8_t tid)
{
    char output[Config::MESSAGE_LEN];
    formatMessage(output, tid, event.c_str(), value.c_str());
    // Serial.println(output);

    loraInstance->setHeaderTo(tid);

    bool rt = loraInstance->print(output);
    if (rt)
    {
        Logger::log(LogLevel::ERROR, "Falha ao enviar comando LoRa");
        return false;
    }

    Logger::info("Enviou");
    Logger::log(LogLevel::DEBUG, String(output).c_str());

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

    String payload = "";
    uint8_t tid = 0xFF; // no future, ler no pacote [1]
    uint8_t pos = 0;
    while (loraInstance->available())
    {
        char buf[241];
        uint8_t len = sizeof(buf);
        bool rt = loraInstance->receiveMessage(buf, len);
        if (!rt)
            return;
        for (uint8_t i = 0; i < len; i++)
        {
            uint8_t byte = buf[i];
            char cbyte = static_cast<char>(byte);
            if (cbyte == '{')
                pos = 0;
            if (pos < 5)
            {
                pos++;
                continue;
            }
            if (!(byte >= 32 && byte <= 126)) // Ignora bytes de controle
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

        int rssi = loraInstance->packetRssi();
        float snr = loraInstance->packetSnr();

        if (rssi < Config::MIN_RSSI_THRESHOLD || snr < Config::MIN_SNR_THRESHOLD)
        {
            Logger::log(LogLevel::WARNING, "Qualidade do link baixa. Tentando ajustar...");
            DisplayManager::displayLowQualityLink(rssi, snr);
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

        if (event != nullptr)
        {
            DeviceInfo::updateDeviceList(tid, payload);
        }

        if (event != nullptr)
        {
            if (String(event) == "status")
            {
                // Apenas atualiza o estado sem acionar comandos extras
                systemState.updateState(String(value));

                // Atualiza Tuya Cloud
                // unsigned char dp_id = 1;
                // unsigned char dp_value = (systemState.getState() == "LIGADO") ? 1 : 0;
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
}