#include "LoRaCom.h"
#include "logger.h"
#include <LoRa.h>
#include "config.h"
#include "system_state.h"
#include "display_manager.h"
#include "device_info.h"
#include <ArduinoJson.h>

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
    // #define SS 18
    // #define RST 14
    // #define DIO0 26
    //     LoRa.setPins(SS, RST, DIO0);

    LoRa.onReceive(onReceiveCallback);
    if (!LoRa.begin(Config::LORA_BAND))
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar LoRa");
        return false;
    }

    //  LoRa.setSyncWord(Config::LORA_SYNC_WORD);
    LoRa.setTxPower(14);            // Potência máxima
    LoRa.setSpreadingFactor(10);    // Spreading Factor
    LoRa.setSignalBandwidth(125E3); // Bandwidth
    LoRa.setCodingRate4(5);         // Coding Rate 4/5
    //  LoRa.setPreambleLength(8);
    //  LoRa.setCodingRate4(4);
    // LoRa.setSpreadingFactor(7);     // SF7 (mais rápido)
    // LoRa.setSignalBandwidth(500e3); // BW = 500 kHz (maior largura = mais velocidade)
    // LoRa.setCodingRate4(4);         // CR = 4/5 (sem redundância extra)
    //  LoRa.setTxPower(5); // TX Power baixo (5 dBm)
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

    /*
    // Feedback visual
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Comando enviado:");
    display.println(event);
    display.display();
    */
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

        if (event != nullptr && String(event) == "presentation")
        {
            DeviceInfo::updateDeviceList(tid, payload);
        }

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
