#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include "Arduino.h"
#include "TimeLib.h"

class Timestamp
{
private:
    time_t timestampInicial = 0;
    unsigned long millisInicial = 0;

    // Variáveis temporárias para leitura dos valores
    int tempYear, tempMonth, tempDay, tempHour, tempMinute, tempSecond;

public:
    /**
     * @brief Converte string ISO8601 para timestamp Unix
     * @param iso String no formato "YYYY-MM-DDTHH:MM:SSZ"
     * @return time_t Timestamp Unix ou 0 em caso de erro
     */
    time_t parseISO8601(const char *iso)
    {
        tmElements_t tm;
        memset(&tm, 0, sizeof(tm));

        // Variáveis temporárias para leitura (int para sscanf)
        int year, month, day, hour, minute, second;

        // Verifica o formato básico
        if (strlen(iso) != 20 || iso[4] != '-' || iso[7] != '-' ||
            iso[10] != 'T' || iso[13] != ':' || iso[16] != ':' || iso[19] != 'Z')
        {
            //      Serial.println("Formato inválido");
            return 0;
        }

        // Extrai os componentes
        if (sscanf(iso, "%d-%d-%dT%d:%d:%dZ",
                   &year, &month, &day, &hour, &minute, &second) != 6)
        {
            //   Serial.println("Falha ao extrair componentes");
            return 0;
        }

        // Validação dos valores
        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
            hour > 23 || minute > 59 || second > 59)
        {
            //  Serial.println("Valores fora do intervalo");
            return 0;
        }

        // Atribui aos membros tmElements_t
        tm.Year = year - 1970; // TimeLib usa anos desde 1970
        tm.Month = month;
        tm.Day = day;
        tm.Hour = hour;
        tm.Minute = minute;
        tm.Second = second;

        time_t result = makeTime(tm);
        if (result == 0)
        {
            Serial.println(iso);
            Serial.println("Falha ao converter para timestamp");
        }
        return result;
    }
    void setCurrentTime(const char *time)
    {
        millisInicial = millis();
        timestampInicial = parseISO8601(time);
        Serial.print("parseISO: ");
        Serial.println(timestampInicial);
        if (timestampInicial == 0)
        {
            // Tratar erro - valor padrão
            timestampInicial = 0;
        }
    }
    static bool isValidTimeFormat(const char *timeStr)
    {
        // Verifica o formato básico "YYYY-MM-DDTHH:MM:SSZ"
        if (strlen(timeStr) != 20)
            return false;
        if (timeStr[4] != '-' || timeStr[7] != '-' ||
            timeStr[10] != 'T' || timeStr[13] != ':' ||
            timeStr[16] != ':' || timeStr[19] != 'Z')
        {
            return false;
        }
        return true;
    }

    time_t getCurrentTime()
    {
        unsigned long elapsedMillis = millis() - millisInicial;
        return timestampInicial + (elapsedMillis / 1000);
    }

    String asString()
    {
        return formatTime(getCurrentTime());
    }

    String formatTime(time_t t)
    {
        char buf[20];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 year(t), month(t), day(t), hour(t), minute(t), second(t));
        return String(buf);
    }
};

extern Timestamp timestamp;

#endif