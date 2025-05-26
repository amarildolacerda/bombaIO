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
    static void info(const char *message)
    {
        log(LogLevel::INFO, message);
    }
    static void warn(const char *message)
    {
        log(LogLevel::WARNING, message);
    }
    static void debug(const char *message)
    {
        log(LogLevel::DEBUG, message);
    }

    static bool log(LogLevel level, const char *format, ...);
    static bool log(LogLevel level, const __FlashStringHelper *message);
    static LogLevel getLevel()
    {
        return currentLogLevel;
    }

private:
    static LogLevel currentLogLevel;
};

#endif // LOGGER_H
