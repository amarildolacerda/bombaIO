#include "logger.h"

#ifdef TIMESTAMP
#include "timestamp.h"
#endif

LogLevel Logger::currentLogLevel = LogLevel::INFO; // Default log level

void Logger::setLogLevel(LogLevel level)
{
    currentLogLevel = level;
}

#ifdef TIMESTAMP

String getISOHour()
{
    return String(timestamp.asString()).substring(11, 19);
}
#endif

bool Logger::log(LogLevel level, const char *format, ...)
{
    if (static_cast<int>(level) > static_cast<int>(currentLogLevel))
    {
        return true; // mesmo quando descartar retorna true para indica que log foi tratado
    }
    static const char levelStrings[][7] PROGMEM = {
        "[ERRO]", "[WARN]", "[RECV]", "[SEND]", "[INFO]",
        "[DBUG]",
        "[VERB]"};

    static const char colorCodes[][8] PROGMEM = {
        "\033[31m", // ERRO - Red
        "\033[34m", // WARN - Blue
        "\033[35m", // RECEIVE - Magenta
        "\033[36m", // SEND - Cyan
        "\033[32m", // INFO - Green
        "\033[33m", // DBUG - Yellow
        "\033[37m"  // VERB - White
    };

    char buffer[128]; // Buffer para a mensagem formatada

    // 1. Processa os argumentos variáveis
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    int idx = static_cast<int>(level);
    char levelBuffer[7];
    char colorBuffer[8];
    strcpy_P(levelBuffer, levelStrings[idx]);
    strcpy_P(colorBuffer, colorCodes[idx]);

    Serial.print(colorBuffer);
    Serial.print(levelBuffer);
#ifdef TIMESTAMP
    Serial.print(F(" "));
    Serial.print(getISOHour());
#endif
    Serial.print(F("-"));

    Serial.println(buffer);
    Serial.print(F("\033[0m")); // Reset color if used
    return true;
}

bool Logger::log(LogLevel level, const __FlashStringHelper *message)
{
    if (message == nullptr)
        return 0;

    char eventBuffer[32];
    strncpy_P(eventBuffer, reinterpret_cast<const char *>(message), sizeof(eventBuffer));

    return Logger::log(level, eventBuffer);
}
