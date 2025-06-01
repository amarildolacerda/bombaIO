#include "logger.h"

// Definição da variável estática
std::function<void(const String &)> Logger::logCallback = nullptr;

void Logger::setLogCallback(std::function<void(const String &)> callback)
{
    logCallback = callback;
}

// Implementações dos outros métodos
void Logger::info(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vlog(LogLevel::INFO, msg, args);
    va_end(args);
}

void Logger::error(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vlog(LogLevel::ERROR, msg, args);
    va_end(args);
}

void Logger::debug(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vlog(LogLevel::DEBUG, msg, args);
    va_end(args);
}

void Logger::warning(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vlog(LogLevel::WARNING, msg, args);
    va_end(args);
}

bool Logger::log(const LogLevel level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    bool result = vlog(level, format, args);
    va_end(args);
    return result;
}

bool Logger::log(LogLevel level, const __FlashStringHelper *message)
{
    if (message == nullptr)
        return false;

    char eventBuffer[32];
    strncpy_P(eventBuffer, reinterpret_cast<const char *>(message), sizeof(eventBuffer));
    return Logger::log(level, eventBuffer);
}
// Implemente todos os outros métodos aqui...
bool Logger::vlog(const LogLevel level, const char *format, va_list args)
{
    char formattedMsg[MAX_LOG_LENGTH];
    vsnprintf(formattedMsg, MAX_LOG_LENGTH, format, args);

    // Sua implementação existente do vlog aqui...

#ifdef DEBUG_ON
    // Versão detalhada com cores (quando DEBUG_ON está definido)
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
    Serial.print(F(" "));
    Serial.print(getISOHour());
#endif
    Serial.print(F(" "));
    Serial.println(formattedMsg);
    Serial.print(F("\033[0m"));
#else
    // Versão simples (quando DEBUG_ON não está definido)
    Serial.println(formattedMsg);
#endif

    if (logCallback)
    {
        String msg = levelBuffer;
        msg += " ";
        msg += formattedMsg;
        logCallback(msg);
    }

    return true;
}