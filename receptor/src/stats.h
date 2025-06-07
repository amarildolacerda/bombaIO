#ifndef STATS_H
#define STATS_H
#include "logger.h"

class Stats
{
public:
    int rxCount = 0;
    int txCount = 0;
    int rxSuccess = 0;
    int txSuccess = 0;

    void print()
    {
        // Logger::log(LogLevel::INFO, "Rx: %d, ok: %d, Tx: %d, ok: %d", rxCount, rxSuccess, txCount, txSuccess);
        Serial.print("RxCount: ");
        Serial.print(rxCount);
        Serial.print(":");
        Serial.print(rxSuccess);
        Serial.print(" Rxs: ");
        Serial.print(rxCount / (millis() / 1000));
        Serial.print(" TxCount: ");
        Serial.print(txCount);
        Serial.print(":");
        Serial.println(txSuccess);
        // txCount = 0;
        // txSuccess = 0;
        // rxCount = 0;
        // rxSuccess = 0;
    }
};

static Stats stats;
#endif