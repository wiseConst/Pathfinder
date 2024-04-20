#ifndef LOG_H
#define LOG_H

#include <fstream>
#include <mutex>
#include <format>
#include <vector>

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

class Logger
{
  public:
    static void Init(const std::string_view& logFileName);
    static void Shutdown();

    template <typename... Args> static void Log(const ELogLevel level, const char* tag, const char* message, Args&&... args)
    {
        std::lock_guard lock(s_LogMutex);

        // Calculate the length of the message with formatted arguments
        const int32_t formattedMessageLength = snprintf(nullptr, 0, message, args...);
        if (formattedMessageLength < 0)
        {
            puts("Error formatting message.");
            return;
        }

        // Fill message with N args
        std::string formattedMessage(formattedMessageLength, 0);
        sprintf(formattedMessage.data(), message, args...);

        // Create structured log message
        const char* systemTimeString = GetCurrentSystemTime();
        std::string logMessage       = {};
        if (tag)
            logMessage = std::format("{} [{}] [{}] {}", systemTimeString, LogLevelToCString(level), tag, formattedMessage);
        else
            logMessage = std::format("{} [{}] {}", systemTimeString, LogLevelToCString(level), formattedMessage);

        // Push message and check if buffer is full and needs to be flushed
        s_LogBuffer.push_back(logMessage);
        if (s_LogBuffer.size() >= s_LogBufferSize) Flush();

        const std::string coloredMessage =
            std::format("{}{}{}", LogLevelToASCIIColor(level), logMessage, LogLevelToASCIIColor(ELogLevel::LOG_LEVEL_NONE));
        puts(coloredMessage.data());
    }

  private:
    static inline std::ofstream s_LogFile;
    static inline std::mutex s_LogMutex;
    static inline std::vector<std::string> s_LogBuffer;
    static inline constexpr uint32_t s_LogBufferSize = 32;  // s_LogBufferSize strings and then flush

    Logger() noexcept = delete;
    ~Logger()         = default;
    static const char* GetCurrentSystemTime();
    static void Flush()
    {
        // Should be called only in Log() and Shutdown().
        if (s_LogFile.is_open())
        {
            for (const auto& message : s_LogBuffer)
                s_LogFile << message << std::endl;

            s_LogFile.flush();    // Ensure the messages are written immediately
            s_LogBuffer.clear();  // Clear the buffer after flushing
        }
    }
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
