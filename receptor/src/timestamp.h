#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include "Arduino.h"
#include "TimeLib.h"

class Timestamp
{
private:
    time_t timestampInicial = 0;     // Timestamp Unix de referência
    unsigned long millisInicial = 0; // Valor de millis() na última sincronização
    float driftCompensation = 1.0f;  // Fator de compensação de drift (1.0 = sem ajuste)
    unsigned long lastSyncTime = 0;  // Última sincronização (em millis)
    time_t lastSyncTimestamp = 0;    // Último timestamp válido recebido

    // Buffer estático para formatação de tempo
    static const size_t TIME_BUFFER_SIZE = 20;
    char timeBuffer[TIME_BUFFER_SIZE];

public:
    /**
     * @brief Converte string ISO8601 para timestamp Unix
     * @param iso String no formato "YYYY-MM-DDTHH:MM:SSZ" ou com offset (max 64 caracteres)
     * @return time_t Timestamp Unix ou 0 em caso de erro
     */
    time_t parseISO8601(const char *iso)
    {
        // Verifica tamanho máximo e ponteiro nulo
        if (!iso || strlen(iso) > 64)
            return 0;

        tmElements_t tm;
        memset(&tm, 0, sizeof(tm));

        int year, month, day, hour, minute, second;
        char offsetSign = '+';
        int offsetHours = 0, offsetMinutes = 0;

        // Verifica formato básico (com 'Z' ou offset)
        if (strlen(iso) < 19 || iso[4] != '-' || iso[7] != '-' ||
            iso[10] != 'T' || iso[13] != ':' || iso[16] != ':')
        {
            return 0;
        }

        // Extrai componentes (permite Z ou offset como +HH:MM/-HH:MM)
        int parsed = sscanf(iso, "%d-%d-%dT%d:%d:%d%c%d:%d",
                            &year, &month, &day, &hour, &minute, &second,
                            &offsetSign, &offsetHours, &offsetMinutes);

        // Requer no mínimo 6 componentes (até os segundos)
        if (parsed < 6)
            return 0;

        // Validação básica
        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
            hour > 23 || minute > 59 || second > 59)
        {
            return 0;
        }

        // Aplica offset se existir (diferente de 'Z')
        int totalOffsetSec = 0;
        if (parsed >= 7 && offsetSign != 'Z')
        {
            totalOffsetSec = (offsetHours * 3600 + offsetMinutes * 60) * (offsetSign == '-' ? 1 : -1);
        }

        // Converte para tmElements_t
        tm.Year = year - 1970;
        tm.Month = month;
        tm.Day = day;
        tm.Hour = hour;
        tm.Minute = minute;
        tm.Second = second;

        return makeTime(tm) + totalOffsetSec;
    }

    /**
     * @brief Define o tempo atual e calcula compensação de drift
     * @param time String ISO8601 com o novo tempo (max 64 caracteres)
     */
    void setCurrentTime(const char *time)
    {
        time_t newTimestamp = parseISO8601(time);
        if (newTimestamp == 0)
        {
            Serial.println(F("Erro: Formato de tempo inválido"));
            return;
        }

        // Calcula compensação de drift se já tiver uma referência
        if (timestampInicial != 0 && millisInicial != 0)
        {
            unsigned long elapsedMillis = millis() - lastSyncTime;
            if (elapsedMillis > 1000) // Só calcula se passou >1s desde a última sync
            {
                driftCompensation = (float)(newTimestamp - lastSyncTimestamp) / (elapsedMillis / 1000.0f);
                driftCompensation = constrain(driftCompensation, 0.9f, 1.1f); // Limita a ±10%
            }

#ifdef DEBUG_ON
            Serial.print(F("Drift compensation: "));
            Serial.println(driftCompensation, 6);
#endif
        }

        // Atualiza referências
        millisInicial = millis();
        timestampInicial = newTimestamp;
        lastSyncTime = millisInicial;
        lastSyncTimestamp = newTimestamp;
#ifdef DEBUG_ON
        Serial.print(F("Tempo atualizado: "));
        formatTime(newTimestamp, timeBuffer, TIME_BUFFER_SIZE);
        Serial.println(timeBuffer);
#endif
    }

    /**
     * @brief Verifica se uma string está no formato ISO8601 básico
     */
    static bool isValidTimeFormat(const char *timeStr)
    {
        if (!timeStr || strlen(timeStr) < 19)
            return false;
        if (timeStr[4] != '-' || timeStr[7] != '-' ||
            timeStr[10] != 'T' || timeStr[13] != ':' ||
            timeStr[16] != ':')
            return false;
        return true;
    }

    /**
     * @brief Retorna o tempo atual com compensação de drift
     */
    time_t getCurrentTime() const
    {
        unsigned long elapsedMillis = millis() - millisInicial;
        return timestampInicial + (time_t)(elapsedMillis * driftCompensation / 1000);
    }

    /**
     * @brief Verifica se precisa de sincronização (útil para NTP periódico)
     * @param syncInterval Intervalo mínimo entre sincronizações (em ms)
     */
    bool needsSync(unsigned long syncInterval = 3600000 /* 1h */) const
    {
        return (millis() - lastSyncTime) > syncInterval;
    }

    /**
     * @brief Retorna o tempo atual como string formatada (usa buffer interno)
     */
    const char *asString()
    {
        formatTime(getCurrentTime(), timeBuffer, TIME_BUFFER_SIZE);
        return timeBuffer;
    }

    /**
     * @brief Formata timestamp Unix como "YYYY-MM-DD HH:MM:SS"
     * @param t Timestamp a ser formatado
     * @param buffer Buffer de saída
     * @param size Tamanho do buffer (deve ser >= 20)
     * @return bool True se formatado com sucesso
     */
    static bool formatTime(time_t t, char *buffer, size_t size)
    {
        if (!buffer || size < TIME_BUFFER_SIZE)
            return false;

        snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
                 year(t), month(t), day(t), hour(t), minute(t), second(t));
        return true;
    }
};

extern Timestamp timestamp;

#endif