#ifndef LOGGER_H
#define LOGGER_H

#include "Arduino.h"
#include <stdarg.h>

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

typedef void (*LogCallback)(const char *);

class Logger
{
private:
    static const size_t MAX_LOG_LENGTH = 255;
    static LogCallback logCallback; // Apenas declaração

public:
    static void setLogCallback(LogCallback callback);

    static void info(const char *msg, ...);
    static void error(const char *msg, ...);
    static void debug(const char *msg, ...);
    static void warning(const char *msg, ...);

    static bool log(LogLevel level, const char *format, ...);
    static bool log(LogLevel level, const __FlashStringHelper *message);

private:
    static bool vlog(const LogLevel level, const char *format, va_list args);
};

#endif