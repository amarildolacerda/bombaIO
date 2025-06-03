#include "LoRaCom.h"
#include "logger.h"

#ifdef ESP32
#include "LoRaTTGO.h"
// #include "LoRa32.h"

#elif RF95
#include <LoRaRF95.h>

#endif

#include "config.h"
#include "display_manager.h"
#include "system_state.h"
#include "device_info.h"
#include "app_messages.h"

// Define the static member
LoRaInterface *LoRaCom::loraInstance = nullptr;

// Define the static receiveCallback pointer
void (*LoRaCom::onReceiveCallback)(MessageRec *) = nullptr;

// ========== LoRaCom Implementations ==========
bool LoRaCom::initialize()
{
#ifdef TTGO
    // setInstance(new LoRa32());
    setInstance(new LoRaTTGO());
#elif RF95
    setInstance(new LoRaRF95());
#endif

#ifdef TTGO
    loraInstance->setPins(Config::LORA_CS_PIN, Config::LORA_RESET_PIN, Config::LORA_IRQ_PIN);
#endif
    bool rt = loraInstance->begin(0, Config::LORA_BAND,
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
    loraInstance->setTerminalId(Config::TERMINAL_ID);
    loraInstance->setTerminalName(Config::TERMINAL_NAME);
    displayManager.setVersion(Config::LMCU_VER);

    loraInstance->endSetup();
    Logger::log(LogLevel::INFO, F("LoRa inicializado com sucesso"));
    systemState.setLoraStatus(true);
    return true;
}

long lastSendTime = 25000;
long lastReceive = 0;
long lastPing = 20000;
uint8_t idPing = 0;

void LoRaCom::handle()
{
    loraInstance->loop();
    MessageRec rec;
    if (loraInstance->processIncoming(rec))
    {
        lastReceive = millis();
        onReceiveCallback(&rec);
    }

#ifdef TTGO
    static uint32_t ultimoRssi = 0;
    if (millis() - ultimoRssi > 1000)
    {
        displayManager.rssi = loraInstance->packetRssi();
        displayManager.snr = loraInstance->packetSnr();
        ultimoRssi = millis();
    }
#endif

    if (millis() - lastSendTime > 30000)
    {
        lastSendTime = millis();
        LoRaCom::sendTime();
    }

    if (millis() - lastReceive > 60000 * 5)
    {
        ESP.restart();
    }
    if (millis() - lastPing > 30000 + idPing * 200)
    {
        lastPing = millis();

        sendCommand(EVT_PING, Config::TERMINAL_NAME, 0xFF);
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
    loraInstance->send(tid, (char *)(ak) ? EVT_ACK : EVT_NAK, "", 0);
    displayManager.showMessage((String)tid + "->" + (ak) ? EVT_ACK : EVT_NAK);
}

int LoRaCom::packetRssi()
{

    int8_t rssi = loraInstance->packetRssi();
    return rssi;
}

void LoRaCom::sendTime()
{
    sendCommand(EVT_TIME, DeviceInfo::getISOTime(), 0xFF);
}

void LoRaCom::sendPresentation(const uint8_t tid, const uint8_t n)
{
    loraInstance->send(tid, EVT_PRESENTATION, "gw", 0);
}

bool LoRaCom::waitAck()
{
    return true;
}

bool LoRaCom::sendCommand(const String event, const String value, uint8_t tid)
{
    loraInstance->send(tid, event.c_str(), value.c_str(), 0);
    displayManager.showMessage((String)(tid) + "->" + event + ": " + value);
    return true;
}

void LoRaCom::setReceiveCallback(void (*callback)(MessageRec *))
{
    onReceiveCallback = callback;
}
