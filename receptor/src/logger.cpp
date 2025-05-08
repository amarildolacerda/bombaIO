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

    static const char *levelStrings[] = {
        "[ERRO]", "[WARN]", "[INFO]"
#if defined(ENABLE_DEBUG)
        ,
        "[DBUG]"
#endif
#if defined(ENABLE_VERBOSE)
        ,
        "[VERB]"
#endif
    };

    static const char *colorCodes[] = {
        "\033[31m", // ERROR - Red
        "\033[34m", // WARNING - Blue
        "\033[32m"  // INFO - Green
#if defined(ENABLE_DEBUG)
        ,
        "\033[33m" // DEBUG - Yellow
#endif
#if defined(ENABLE_VERBOSE)
        ,
        "\033[36m" // VERBOSE - Cyan
#endif
    };

    int idx = static_cast<int>(level);
    Serial.print(colorCodes[idx]);
    Serial.print(levelStrings[idx]);
    Serial.print(" ");
    Serial.println(message);
    Serial.print("\033[0m"); // Reset color if used
}
