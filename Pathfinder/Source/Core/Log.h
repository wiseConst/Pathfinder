#ifndef LOG_H
#define LOG_H

#include "Core.h"

#include <fstream>
#include <mutex>

namespace Pathfinder
{

enum class ELogLevel : uint8_t
{
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

static const char* LogLevelToCString(const ELogLevel level)
{
    switch (level)
    {
        case ELogLevel::LOG_LEVEL_ERROR: return "ERROR";
        case ELogLevel::LOG_LEVEL_WARN: return "WARNING";
        case ELogLevel::LOG_LEVEL_TRACE: return "TRACE";
        case ELogLevel::LOG_LEVEL_INFO: return "INFO";
        case ELogLevel::LOG_LEVEL_DEBUG: return "DEBUG";
    }

    return "Untagged";
}

static const char* LogLevelToASCIIColor(const ELogLevel level)
{
    switch (level)
    {
        case ELogLevel::LOG_LEVEL_ERROR: return "\x1b[91m";
        case ELogLevel::LOG_LEVEL_WARN: return "\x1b[93m";
        case ELogLevel::LOG_LEVEL_TRACE: return "\x1b[92m";
        case ELogLevel::LOG_LEVEL_INFO: return "\x1b[94m";
        case ELogLevel::LOG_LEVEL_DEBUG: return "\x1b[96m";
    }

    return "\033[0m";
}

class Logger final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    template <typename... Args>
    static void Log(const ELogLevel level, const char* tag, const char* message, Args&&... args)
    {
        std::lock_guard lock(s_LogMutex);

        static constexpr uint32_t s_MaxMessageLength        = 16384;
        static char s_TempMessageBuffer[s_MaxMessageLength] = {0};
        memset(s_TempMessageBuffer, 0, sizeof(s_TempMessageBuffer[0]) * s_MaxMessageLength);

        if (strlen(message) > s_MaxMessageLength)
        {
            puts("TODO: Logger message length resize?");
            puts(message);
            return;
        }

        // Fill message with N args
        char formattedMessage[s_MaxMessageLength] = {0};
        sprintf(formattedMessage, message, args...);
        const char* systemTimeString = GetCurrentSystemTime();

        // Create structured log message
        if (tag)
            sprintf(s_TempMessageBuffer, "%s [%s]: [%s]: %s", systemTimeString, LogLevelToCString(level), tag, formattedMessage);
        else
            sprintf(s_TempMessageBuffer, "%s [%s]: %s", systemTimeString, LogLevelToCString(level), formattedMessage);

        // Non-colored output into log
        s_LogFile << s_TempMessageBuffer << std::endl;

        // Add colors
        sprintf(formattedMessage, "%s%s%s", LogLevelToASCIIColor(level), s_TempMessageBuffer,
                LogLevelToASCIIColor(ELogLevel::LOG_LEVEL_NONE));
        puts(formattedMessage);
    }

  private:
    static inline std::ofstream s_LogFile;
    static inline std::mutex s_LogMutex;

    Logger() noexcept  = default;
    ~Logger() override = default;
    static const char* GetCurrentSystemTime();
};

}  // namespace Pathfinder

#define LOG_ERROR(msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_ERROR, nullptr, msg, ##__VA_ARGS__)
#define LOG_WARN(msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_WARN, nullptr, msg, ##__VA_ARGS__)

#define LOG_TRACE(msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_TRACE, nullptr, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_INFO, nullptr, msg, ##__VA_ARGS__)

#if PFR_DEBUG
#define LOG_DEBUG(msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_DEBUG, nullptr, msg, ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...) (msg)
#endif

#define LOG_TAG_INFO(tag, msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_INFO, #tag, msg, ##__VA_ARGS__)
#define LOG_TAG_TRACE(tag, msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_TRACE, #tag, msg, ##__VA_ARGS__)
#define LOG_TAG_WARN(tag, msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_WARN, #tag, msg, ##__VA_ARGS__)
#define LOG_TAG_ERROR(tag, msg, ...) Logger::Log(ELogLevel::LOG_LEVEL_ERROR, #tag, msg, ##__VA_ARGS__)

#endif  // LOG_H
