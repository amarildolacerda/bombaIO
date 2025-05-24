#include "logger.h"

LogLevel Logger::currentLogLevel = LogLevel::INFO; // Default log level

void Logger::setLogLevel(LogLevel level)
{
    currentLogLevel = level;
}

String getISOHour()
{
#ifdef __AVR__
    return "1970-01-01T00:00:00Z"; // Fallback for AVR
#else

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return "1970-01-01T00:00:00Z";
    }

    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    return String(timeStr);
#endif
}

bool Logger::log(LogLevel level, const char *message)
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

    int idx = static_cast<int>(level);
    char levelBuffer[7];
    char colorBuffer[8];
    strcpy_P(levelBuffer, levelStrings[idx]);
    strcpy_P(colorBuffer, colorCodes[idx]);

    Serial.print(colorBuffer);
    Serial.print(levelBuffer);
    Serial.print(F(" "));
    Serial.print(getISOHour());
    Serial.print(F("-"));

    Serial.println(message);
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
bool Logger::log(LogLevel level, String message)
{
    return log(level, message.c_str());
}
