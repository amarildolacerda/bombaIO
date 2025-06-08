#include "LoRaCom.h"
#include "logger.h"

#ifndef LORA
#include "loradummy.h"
#else

#ifdef TTGO
#include "LoRaTTGO.h"
#endif
#ifdef RF95
#include <LoRaRF95.h>
#endif
#ifdef HELTEC
#include <LoRa32.h>
#endif
#endif

#include "config.h"
#ifdef DISPLAY_ENABLED
#include "display_manager.h"
#endif
#include "system_state.h"
#include "device_info.h"
#include "app_messages.h"

// Define the static member
LoRaInterface *LoRaCom::loraInstance = nullptr;
bool LoRaCom::loraConnected = false;

// Define the static receiveCallback pointer
void (*LoRaCom::onReceiveCallback)(MessageRec *) = nullptr;

// ========== LoRaCom Implementations ==========
// Add this helper function to set the loraInstance pointer
void LoRaCom::setInstance(LoRaInterface *instance)
{
    loraInstance = instance;
}

bool LoRaCom::initialize()
{

#ifndef LORA
    setInstance(new LoRaDummy());
#else
#ifdef TTGO
    // setInstance(new LoRa32());
    setInstance(new LoRaTTGO());
#elif RF95
    setInstance(new LoraRF());
#endif
#ifdef HELTEC
    setInstance(new LoRa32());
#endif
#endif

    loraInstance->config = LORA_MED;

    loraInstance->setPins(Config::LORA_CS_PIN, Config::LORA_RESET_PIN, Config::LORA_IRQ_PIN);
    loraConnected = loraInstance->begin(0, Config::LORA_BAND, true);
    if (!loraConnected)
    {
        Logger::log(LogLevel::ERROR, "Falha ao iniciar LoRa");
        return false;
    }
    loraInstance->setTerminalId(Config::TERMINAL_ID);
    loraInstance->setTerminalName(Config::TERMINAL_NAME);

#ifdef DISPLAY_ENABLED
    displayManager.setVersion(Config::LMCU_VER);
#endif
    loraInstance->endSetup();
    Logger::log(LogLevel::INFO, "LoRa inicializado com sucesso");
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

#ifdef DISPLAY_ENABLED

    static uint32_t ultimoRssi = 0;
    if (millis() - ultimoRssi > 1000)
    {
        displayManager.rssi = loraInstance->packetRssi();
        displayManager.snr = loraInstance->packetSnr();
        displayManager.loraConnected = loraConnected;
        displayManager.handle();
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
        // ESP.restart();
    }
    if (millis() - lastPing > Config::COMMAND_TIMEOUT)
    {
        lastPing = millis();

        sendCommand(EVT_PING, Config::TERMINAL_NAME, 0xFF);
    }
}

void LoRaCom::formatMessage(char *message, uint8_t tid, const char *event, const char *value)
{
    int n = sizeof(message);
    memset(message, 0, n);
    sprintf(message, "%s|%s", event, value);
}

void LoRaCom::ack(bool ak, uint8_t tid)
{
    loraInstance->send(tid, (char *)(ak) ? EVT_ACK : EVT_NAK, "", 0);
#ifdef DISPLAY_ENABLED
    displayManager.showMessage((String)tid + "->" + (ak) ? EVT_ACK : EVT_NAK);
#endif
}

int LoRaCom::packetRssi()
{

    int8_t rssi = loraInstance->packetRssi();
    return rssi;
}

void LoRaCom::sendTime()
{
    // sendCommand(EVT_TIME, DeviceInfo::getISOTime(), 0xFF);
    // suspenso, esta travando o client
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
#ifdef DISPLAY_ENABLED
    displayManager.showMessage((String)(tid) + "->" + event + ": " + value);
#endif
    return true;
}

void LoRaCom::setReceiveCallback(void (*callback)(MessageRec *))
{
    onReceiveCallback = callback;
}
