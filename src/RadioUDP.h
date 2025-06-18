#ifndef UDP_INTERFACE_H
#define UDP_INTERFACE_H

#include <RadioInterface.h>
#include <ESPAsyncUDP.h>
// #include <WiFiUdp.h>
#include "logger.h"
#include "stats.h"

class RadioUDP : public RadioInterface
{
private:
    AsyncUDP udp;
    uint16_t _localPort;
    IPAddress _broadcastIP;
    bool _isServer;
    bool _initialized;

public:
    RadioUDP(uint16_t localPort = 1234, bool isServer = false) : _localPort(localPort), _isServer(isServer), _initialized(false)
    {
        _broadcastIP = IPAddress(255, 255, 255, 255);
    }

    bool sendMessage(MessageRec &rec) override
    {
        if (!_initialized)
            return false;

        stats.txCount++;
        char message[MESSAGE_MAX_LEN] = {0};
        size_t result = rec.encode(message, MESSAGE_MAX_LEN);

        if (result > 0)
        {
            if (_isServer)
            {
                // Servidor envia para broadcast
                udp.broadcastTo((uint8_t *)message, result, _localPort);
            }
            else
            {
                // Cliente envia para servidor
                udp.broadcastTo((uint8_t *)message, result, _localPort);
            }

            stats.txSuccess++;
            log(true, rec);
            return true;
        }
        return false;
    }

    bool receiveMessage() override
    {
        // O recebimento é tratado no callback do AsyncUDP
        return false;
    }

    void modeRx() override
    {
        // Não há ação específica para modo RX no UDP
    }

    void modeTx() override
    {
        // Não há ação específica para modo TX no UDP
    }

    bool begin(const uint8_t terminal_id, uint16_t port)
    {
        _localPort = port;
        return begin(terminal_id, 0, true);
    }
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;

        if (udp.listen(_localPort))
        {
            udp.onPacket([this](AsyncUDPPacket packet)
                         { this->handleUDPPacket(packet); });

            connected = true;
            _initialized = true;

#ifdef DEBUG_ON
            Logger::info("UDP Interface iniciada na porta %d", _localPort);
#endif

            return true;
        }

#ifdef DEBUG_ON
        Logger::error("Falha ao iniciar UDP na porta %d", _localPort);
#endif

        return false;
    }

    void handleUDPPacket(AsyncUDPPacket &packet)
    {
        stats.rxCount++;

        if (packet.length() >= MESSAGE_MAX_LEN)
        {
#ifdef DEBUG_ON
            Logger::error("Pacote UDP muito grande: %d bytes", packet.length());
#endif
            return;
        }

        MessageRec rec;
        char resp[MESSAGE_MAX_LEN] = {0};
        memcpy(resp, packet.data(), packet.length());
        Serial.println(resp + 5);
        Serial.println(packet.length());
        if (packet.length() < MESSAGE_MAX_LEN)
        {
            // Print message content starting from offset 5; adjust offset if necessary
            bool result = rec.decode(resp, packet.length());

            if (!result && rec.from == terminalId)
            {
#ifdef DEBUG_ON
                Logger::error("Pacote UDP mal formado");
#endif
                return;
            }

            if (rec.from == terminalId)
            {
                return; // skip messages from myself
            }

            addRxMessage(rec);
            meshMessage(rec);
        }
    }
    void setBroadcastIP(IPAddress ip)
    {
        _broadcastIP = ip;
    }

    int packetRssi() override
    {
        // UDP não tem RSSI
        return 0;
    }

    int packetSnr() override
    {
        // UDP não tem SNR
        return 0;
    }

    virtual String getIdent() override
    {
        return "UDP";
    }
};

#endif // UDP_INTERFACE_H