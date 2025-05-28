#ifndef LOGGER_H
#define LOGGER_H

#include "Arduino.h"
#include <stdarg.h>

//--------------------------------------------------LOG
enum class LogLevel
{
    ERROR,
    WARNING,
    RECEIVE,
    SEND,
    INFO,
    DEBUG,
    VERBOSE
};

class Logger
{
private:
    static const size_t MAX_LOG_LENGTH = 128; // Maximum length of log message

public:
    static void info(const char *msg, ...)
    {
        va_list args;
        va_start(args, msg);
        log(LogLevel::INFO, msg, args);
        va_end(args);
    }

    static void error(const char *msg, ...)
    {
        va_list args;
        va_start(args, msg);
        log(LogLevel::ERROR, msg, args);
        va_end(args);
    }

    static void debug(const char *msg, ...)
    {
        va_list args;
        va_start(args, msg);
        log(LogLevel::DEBUG, msg, args);
        va_end(args);
    }

    static void warning(const char *msg, ...)
    {
        va_list args;
        va_start(args, msg);
        log(LogLevel::WARNING, msg, args);
        va_end(args);
    }

    static bool log(const LogLevel level, const char *msg, ...)
    {
        va_list args;
        va_start(args, msg);
        bool result = vlog(level, msg, args);
        va_end(args);
        return result;
    }

    static bool vlog(const LogLevel level, const char *msg, va_list args)
    {
        char formattedMsg[MAX_LOG_LENGTH];
        vsnprintf(formattedMsg, MAX_LOG_LENGTH, msg, args);

#ifndef DEBUG_ON
        Serial.println(formattedMsg);
        return true;
#else
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
#ifdef TIMESTAMP
//        Serial.print(F(" "));
//        Serial.print(getISOHour());
#endif
        Serial.print(F("-"));
        Serial.println(formattedMsg);
        Serial.print(F("\033[0m")); // Reset color if used
        return true;
#endif
    }
};

#endif