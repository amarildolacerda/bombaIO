#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

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
    static void setLogLevel(LogLevel level);
    static void error(const char *message)
    {
        log(LogLevel::ERROR, message);
    }
    static void error(String message)
    {
        log(LogLevel::ERROR, message.c_str());
    }
    static void info(const char *message)
    {
        log(LogLevel::INFO, message);
    }
    static void info(String message)
    {
        log(LogLevel::INFO, message.c_str());
    }
    static void warn(const char *message)
    {
        log(LogLevel::WARNING, message);
    }
    static void debug(const char *message)
    {
        log(LogLevel::DEBUG, message);
    }

    static bool log(LogLevel level, const char *message);
    static bool log(LogLevel level, const __FlashStringHelper *message);
    bool log(LogLevel level, String message);

private:
    static LogLevel currentLogLevel;
};

#endif // LOGGER_H
