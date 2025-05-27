
#ifndef LOGGER_H
#define LOGGER_H

#include "Arduino.h"
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
public:
    static void info(String msg)
    {
        log(LogLevel::INFO, msg);
    }
    static void error(String msg)
    {
        log(LogLevel::ERROR, msg);
    }
    static bool log(const LogLevel level, const String msg)
    {

#ifndef DEBOG_ON
        Serial.println(msg);
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

        Serial.println(msg);
        Serial.print(F("\033[0m")); // Reset color if used
        return true;
#endif
    }
};

#endif