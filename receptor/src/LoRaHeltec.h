#ifndef LORAHELTEC_H
#define LORAHELTEC_H
#include "LoRaWan_APP.h"
// #include "radio.h"
#include "Arduino.h"
#include "LoRaInterface.h"
#include "stats.h"
#include "config.h"
#include "logger.h"

#define RF_FREQUENCY Config::LORA_BAND // Hz

#define TX_OUTPUT_POWER 20 // dBm

#define LORA_BANDWIDTH 0        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 9 // [SF7..SF12]
#define LORA_CODINGRATE 1       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

class LoRaHeltec : public LoRaInterface
{
private:
public:
    int16_t rssi = -157;
    void loop() override
    {
        // Serial.println("LOOP");
        if (state == LoRaIDLE)
        {
            Radio.IrqProcess();
            setState(LoRaRX);
        }
        LoRaInterface::loop();
        // Serial.println("END-LOOP");
        return;
    }
    bool sendMessage(MessageRec &rec) override
    {

        stats.txCount++;
        char txpacket[Config::MESSAGE_MAX_LEN];
        int len = sprintf(txpacket, "%c%c%c%c%c{%s|%s}",
                          rec.from, rec.to, rec.id, rec.hope, terminalId,
                          rec.event, rec.value);

        // Serial.printf("[SEND] \"%s\" , len %d\r\n", txpacket + 5, len);
        Radio.Send((uint8_t *)txpacket, len);
        Logger::log(LogLevel::SEND, "(%d)[%d-%d:%d](%d) %s",
                    terminalId, rec.from, rec.to, rec.id, rec.hope,
                    txpacket + 5);

        Radio.IrqProcess();

        setState(LoRaIDLE);
        return true;
    }
    bool receiveMessage() override
    {
        //        Serial.println("receiveMessage");
        stats.rxCount++;
        Radio.Rx(0);
        return false;
    };
    void modeRx() override
    {
        Radio.Rx(0);
    };
    void modeTx() override {
    };
    bool begin(const uint8_t terminal_Id, long band, bool promisc = true) override
    {
        terminalId = terminal_Id;

        Mcu.begin(); // HELTEC_BOARD, SLOW_CLK_TPYE);
        // txNumber = 0;
        // Rssi = 0;

        RadioEvents.TxDone = OnTxDone;
        RadioEvents.TxTimeout = OnTxTimeout;
        RadioEvents.RxDone = OnRxDone;

        Radio.Init(&RadioEvents);
        Radio.SetChannel(RF_FREQUENCY);
        Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                          LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                          LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                          true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

        Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                          LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                          LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                          0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
        state = LoRaRX;
        connected = Radio.GetStatus() == RF_IDLE;

        return true;
    };
    int packetRssi() override
    {
        return rssi;
    }

    uint8_t headerSender;
    bool parseRecv(char *r, uint8_t len, MessageRec &rec)
    {
        memset(&rec, 0, sizeof(MessageRec));
        rec.to = r[1];
        rec.from = r[0];
        rec.id = r[2];
        rec.hope = r[3];
        headerSender = r[4];

        char buf[Config::MESSAGE_MAX_LEN] = {0};
        sprintf(buf, r + 5, len - 5);
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
    void rxAdd(MessageRec &rec)
    {
        rxQueue.pushItem(rec);
    }
};

static LoRaHeltec lora;

void OnTxDone(void)
{
    stats.txSuccess++;
    Serial.println("OnTxDone");
}
void OnTxTimeout(void)
{
    Serial.println("OnTxTimeout");
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    lora.rssi = rssi;
    stats.rxSuccess++;
    Serial.print("OnRxDone ");
    // Serial.println((char *)payload);
    Serial.print("Size: ");
    Serial.println(size);
    MessageRec rec;
    if (lora.parseRecv((char *)payload, size, rec))
    {
        lora.rxAdd(rec);
    }
}

#endif