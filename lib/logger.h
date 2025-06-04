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

#ifdef __AVR__
typedef void (*LogCallback)(const char *);
#endif

class Logger
{
private:
    static const size_t MAX_LOG_LENGTH = 255;
#ifdef __AVR__
    static LogCallback logCallback; // Apenas declaração
#else
    static std::function<void(const String &)> logCallback; // Apenas declaração
#endif
public:
#ifdef __AVR__
    static void setLogCallback(LogCallback callback);

#else
    static void setLogCallback(std::function<void(const String &)> callback);
#endif
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