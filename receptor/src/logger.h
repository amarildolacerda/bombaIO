#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum class LogLevel
{
    ERROR,
    WARNING,
    INFO,
#if defined(ENABLE_DEBUG)
    DEBUG,
#endif
#if defined(ENABLE_VERBOSE)
    VERBOSE
#endif
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

    static void log(LogLevel level, const char *message);

private:
    static LogLevel currentLogLevel;
};

#endif // LOGGER_H
