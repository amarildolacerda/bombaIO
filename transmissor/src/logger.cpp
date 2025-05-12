#include "logger.h"

LogLevel Logger::currentLogLevel = LogLevel::INFO; // Default log level

void Logger::setLogLevel(LogLevel level)
{
    currentLogLevel = level;
}

void Logger::log(LogLevel level, const char *message)
{
    if (static_cast<int>(level) > static_cast<int>(currentLogLevel))
    {
        return; // Skip logging if the level is below the current log level
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
    Serial.println(message);
    Serial.print(F("\033[0m")); // Reset color if used
}

void Logger::log(LogLevel level, const __FlashStringHelper *message)
{
    if (message == nullptr)
    {
        Serial.print(F("[WARN] Empty or null log message\n"));
        return;
    }

    if (static_cast<int>(level) > static_cast<int>(currentLogLevel))
    {
        return; // Skip logging if the level is below the current log level
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
    Serial.println(message);
    Serial.print(F("\033[0m")); // Reset color if used
}
