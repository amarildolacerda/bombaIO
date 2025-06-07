#ifndef LORA32_H
#define LORA32_H

#include "Arduino.h"
#include "queue_message.h"
#include "config.h"
#include "logger.h"
#include "LoRaInterface.h"
#include "stats.h"

#ifdef ESP32
#include "SPI.h"
#if defined(HELTEC)
#include "heltec.h"
#ifdef Heltec_LoRa
#define LoRa Heltec.LoRa
#else
#include "lora/LoRa.h"
#endif
#else
#include "LoRa.h"
#endif

#define ALIVE_PACKET 3

class LoRaAutoTuner
{
private:
    uint8_t currentSF;
    long currentBW;
    uint8_t currentCR;
    uint8_t currentPower;
    bool isHeltec;

    // Métricas
    float rssiSamples[5] = {0};
    float snrSamples[5] = {0};
    uint8_t sampleIndex = 0;
    uint16_t lostPackets = 0;

public:
    uint16_t totalPackets = 0;
    LoRaAutoTuner(uint8_t sf, long bw, uint8_t cr, uint8_t power, bool heltec)
        : currentSF(sf), currentBW(bw), currentCR(cr), currentPower(power), isHeltec(heltec) {}

    void updateMetrics(float rssi, float snr, bool packetLost)
    {
        rssiSamples[sampleIndex] = rssi;
        snrSamples[sampleIndex] = snr;
        sampleIndex = (sampleIndex + 1) % 5;

        totalPackets++;
        if (packetLost)
            lostPackets++;
    }

    void adjustParameters()
    {
        float avgRSSI = getAverageRSSI();
        float avgSNR = getAverageSNR();
        float lossRate = getPacketLossRate();

        // Ajuste para Heltec (com PABOOST)
        if (isHeltec)
        {
            if (avgRSSI > -60 && currentPower > 10)
            {
                currentPower -= 2;
#if HELTEC
                LoRa.setTxPower(currentPower, RF_PACONFIG_PASELECT_PABOOST);
#else
                LoRa.setTxPower(currentPower);
#endif
                Logger::log(LogLevel::INFO, "[Heltec] Reduzindo potência para %d dBm", currentPower);
            }
            else if (avgRSSI < -90 || avgSNR < 0)
            {
                if (currentSF < 12 && lossRate > 10)
                {
                    currentSF++;
                    LoRa.setSpreadingFactor(currentSF);
                    Logger::log(LogLevel::INFO, "[Heltec] Aumentando SF para %d", currentSF);
                }
                else if (currentPower < 20)
                {
                    currentPower += 2;
#ifdef HELTEC
                    LoRa.setTxPower(currentPower, RF_PACONFIG_PASELECT_PABOOST);
#else
                    LoRa.setTxPower(currentPower);
#endif
                    Logger::log(LogLevel::INFO, "[Heltec] Aumentando potência para %d dBm", currentPower);
                }
            }
        }
        // Ajuste para TTGO comum
        else
        {
            if (avgRSSI > -60 && currentPower > 10)
            {
                currentPower -= 2;
                LoRa.setTxPower(currentPower
#ifdef HELTEC
                                ,
                                RF_PACONFIG_PASELECT_PABOOST
#endif
                );
                Logger::log(LogLevel::INFO, "[TTGO] Reduzindo potência para %d dBm", currentPower);
            }
            else if (avgRSSI < -90 || avgSNR < 0)
            {
                if (currentSF < 12 && lossRate > 10)
                {
                    currentSF++;
                    LoRa.setSpreadingFactor(currentSF);
                    Logger::log(LogLevel::INFO, "[TTGO] Aumentando SF para %d", currentSF);
                }
                else if (currentPower < 14)
                { // TTGO geralmente tem limite menor
                    currentPower += 2;
                    LoRa.setTxPower(currentPower
#ifdef HELTEC
                                    ,
                                    RF_PACONFIG_PASELECT_PABOOST
#endif
                    );
                    Logger::log(LogLevel::INFO, "[TTGO] Aumentando potência para %d dBm", currentPower);
                }
            }
        }

        // Ajustes comuns a ambos
        if (lossRate > 5 && currentBW < 250E3)
        {
            currentBW = (currentBW == 125E3) ? 250E3 : 500E3;
            LoRa.setSignalBandwidth(currentBW);
            Logger::log(LogLevel::INFO, "Aumentando BW para %ld Hz", currentBW);
        }
        else if (avgRSSI > -80 && avgSNR > 7 && lossRate < 2)
        {
            if (currentSF > 7)
            {
                currentSF--;
                LoRa.setSpreadingFactor(currentSF);
                Logger::log(LogLevel::INFO, "Reduzindo SF para %d para melhorar throughput", currentSF);
            }
            else if (currentBW == 125E3 && currentSF <= 9)
            {
                currentBW = 250E3;
                LoRa.setSignalBandwidth(currentBW);
                Logger::log(LogLevel::INFO, "Aumentando BW para %ld Hz para melhorar throughput", currentBW);
            }
        }
    }

    float getAverageRSSI()
    {
        float sum = 0;
        for (int i = 0; i < 5; i++)
            sum += rssiSamples[i];
        return sum / 5;
    }

    float getAverageSNR()
    {
        float sum = 0;
        for (int i = 0; i < 5; i++)
            sum += snrSamples[i];
        return sum / 5;
    }

    float getPacketLossRate()
    {
        if (totalPackets == 0)
            return 0.0;
        return (lostPackets * 100.0) / totalPackets;
    }

    void resetCounters()
    {
        totalPackets = 0;
        lostPackets = 0;
    }

    uint8_t getCurrentSF() const { return currentSF; }
    long getCurrentBW() const { return currentBW; }
    uint8_t getCurrentCR() const { return currentCR; }
    uint8_t getCurrentPower() const { return currentPower; }
};

class LoRa32 : public LoRaInterface
{
private:
    bool isHeltec;
    LoRaAutoTuner autoTuner;
    bool lastPacketLost = false;

    uint8_t _headerTo;
    uint8_t _headerFrom;
    uint8_t _headerId;
    uint8_t _headerSender;
    uint8_t _headerHope;
    long lastRcvMesssage;

public:
    LoRa32() : isHeltec(false), autoTuner(LORA_SF, LORA_BW, LORA_CR, LORA_PW, false) {}

    void modeTx() override {}
    void modeRx() override { LoRa.receive(); }

    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        _promiscuos = promisc;
        terminalId = terminal_Id;

#ifdef HELTEC
        isHeltec = true;
        autoTuner = LoRaAutoTuner(LORA_SF, LORA_BW, LORA_CR, LORA_PW, true); // Valores iniciais para Heltec

        Heltec.begin(true, false, true);
        delay(100);

        if (LoRa.begin(band, true))
        { // PABOOST ativado
            connected = true;
            Serial.println("LoRa OK");
        }
        else
        {
            Serial.println("LoRa nao conectou");
            return false;
        }
#else
        isHeltec = false;
        if (!LoRa.begin(band))
        {
            return false;
        }
#endif

        // Aplica configurações iniciais do autoTuner
        LoRa.setSpreadingFactor(autoTuner.getCurrentSF());
        LoRa.setSignalBandwidth(autoTuner.getCurrentBW());
        LoRa.setCodingRate4(autoTuner.getCurrentCR());

#ifdef HELTEC
        LoRa.setTxPower(autoTuner.getCurrentPower(), RF_PACONFIG_PASELECT_PABOOST);
        LoRa.setTxPowerMax(20);
#else
        LoRa.setTxPower(autoTuner.getCurrentPower());
#endif

        LoRa.setSyncWord(Config::LORA_SYNC_WORD);
        LoRa.setPreambleLength(8);
        LoRa.enableCrc();

        setState(LoRaRX);
        Logger::log(LogLevel::INFO, "LoRa32 iniciado (Heltec: %d)", isHeltec);
        return true;
    }

    bool sendMessage(MessageRec &rec) override
    {
        stats.txCount++;
        char message[Config::MESSAGE_MAX_LEN] = {0};
        uint8_t len = snprintf(message, sizeof(message), "{%s|%s}", rec.event, rec.value);

        // LoRa.idle();
        LoRa.beginPacket();

        LoRa.write(rec.to);
        LoRa.write(rec.from);
        LoRa.write(rec.id);
        LoRa.write(rec.hope);
        LoRa.write(terminalId);

        LoRa.print(message);

        bool result = LoRa.endPacket(true) > 0;

        if (result)
        {
            stats.txSuccess++;
            Logger::log(LogLevel::SEND, "(%d)[%X→%X:%X](%d) %s",
                        terminalId, rec.from, rec.to, rec.id, rec.hope, message);

            // Atualiza métricas (assume sucesso no envio)
            autoTuner.updateMetrics(LoRa.packetRssi(), LoRa.packetSnr(), false);
            checkAutoTune();
        }
        else
        {
            lastPacketLost = true;
        }

        return result;
    }

    bool receiveMessage() override
    {
        stats.rxCount++;

        uint8_t packetSize = LoRa.parsePacket();
        if (packetSize < 5)
        {
            lastPacketLost = true;
            return false;
        }

        _headerTo = LoRa.read();
        _headerFrom = LoRa.read();
        _headerId = LoRa.read();
        _headerHope = LoRa.read();
        _headerSender = LoRa.read();

        char buffer[Config::MESSAGE_MAX_LEN];
        uint8_t len = 0;
        bool passouPipe = false;
        long waitingRcv = millis();

        while (len <= packetSize + 5)
        {
            if (!LoRa.available() && (millis() - waitingRcv > 100))
            {
                lastPacketLost = true;
                break;
            }
            uint8_t r = LoRa.read();
            buffer[len++] = (char)r;
            waitingRcv = millis();
        }
        stats.rxSuccess++;
        buffer[len] = '\0';

        MessageRec rec;
        if (!parseRecv(buffer, len, rec))
        {
            lastPacketLost = true;
            return false;
        }

        if (len == 0 || _headerFrom == terminalId || _headerSender == terminalId)
        {
            lastPacketLost = true;
            return false;
        }

        // Tratamento de mensagens especiais (ping/pong)
        if (strstr(buffer, EVT_PING) != NULL)
        {
            if (_headerTo != terminalId)
                return false;
            txQueue.push(_headerFrom, EVT_PONG, terminalName, terminalId, ALIVE_PACKET, _headerId);
            return true;
        }

        bool handled = (_headerTo == 0xFF || _headerTo == terminalId);

        // Atualiza métricas
        autoTuner.updateMetrics(LoRa.packetRssi(), LoRa.packetSnr(), lastPacketLost);
        checkAutoTune();
        lastPacketLost = false;

        if (handled)
        {
            if (!rxQueue.pushItem(rec))
            {
                handled = false;
            }
            Logger::log(handled ? LogLevel::RECEIVE : LogLevel::WARNING,
                        "(%d)[%X→%X:%X](%d) %s|%s",
                        _headerSender, _headerFrom, _headerTo, _headerId, _headerHope,
                        rec.event, rec.value);
        }

        return handled;
    }

    void checkAutoTune()
    {

        static uint32_t lastCheck = 0;
        if (millis() - lastCheck > 30000 || autoTuner.totalPackets >= 50)
        {
            if (LORA_AUTO)
                autoTuner.adjustParameters();
            Logger::log(LogLevel::INFO, "Config: SF%d BW%ld CR%d PWR%d",
                        autoTuner.getCurrentSF(), autoTuner.getCurrentBW(),
                        autoTuner.getCurrentCR(), autoTuner.getCurrentPower());
            Logger::log(LogLevel::INFO, "Qualidade: RSSI %.1f SNR %.1f Perda %.1f%%",
                        autoTuner.getAverageRSSI(), autoTuner.getAverageSNR(),
                        autoTuner.getPacketLossRate());
            autoTuner.resetCounters();
            lastCheck = millis();
        }
    }

    bool parseRecv(char *buf, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = _headerTo;
        rec.from = _headerFrom;
        rec.id = _headerId;
        rec.hope = _headerHope;

        if (buf == NULL || buf[0] != '{' || buf[len - 1] != '}')
        {
            return false;
        }

        char content[100];
        strncpy(content, buf + 1, len - 2);
        content[len - 2] = '\0';

        char *token = strtok(content, "|");
        if (token != nullptr)
        {
            snprintf(rec.event, sizeof(rec.event), token);
        }

        token = strtok(nullptr, "|");
        if (token != nullptr)
        {
            snprintf(rec.value, sizeof(rec.value), token);
        }
        else
        {
            rec.value[0] = '\0';
        }
        return true;
    }

    void setPins(const uint8_t cs, const uint8_t reset, const uint8_t irq) override
    {

        LoRa.setPins(cs, reset, irq);
        if (isHeltec)
            SPI.begin(5, 19, 27, 18); // SCK, MISO, MOSI, SS
    }

    int packetRssi() override
    {
        return LoRa.packetRssi();
    }

    int packetSnr() override
    {
        return LoRa.packetSnr();
    }
};

static LoRa32 lora;
#endif
#endif