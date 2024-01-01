#include "PathfinderPCH.h"
#include "Log.h"

namespace Pathfinder
{

static bool s_bIsLoggerInitialized = false;

void Logger::Init()
{
    if (!s_bIsLoggerInitialized)
    {
        s_LogFile.open("Pathfinder.log", std::ios::out | std::ios::trunc);
        PFR_ASSERT(s_LogFile.is_open(), "Failed to open log file!");

        s_bIsLoggerInitialized = true;
    }
}

void Logger::Shutdown()
{
    if (s_bIsLoggerInitialized)
    {
        s_LogFile.close();
        s_bIsLoggerInitialized = false;
    }
}

const char* Logger::GetCurrentSystemTime()
{
    static constexpr uint32_t s_MaxSystemTimeMessageLength              = 64 + 1;
    static char s_SystemTimeMessageBuffer[s_MaxSystemTimeMessageLength] = {0};
    memset(s_SystemTimeMessageBuffer, 0, sizeof(char) * s_MaxSystemTimeMessageLength);

    const auto now     = std::chrono::system_clock::now();
    const auto rawTime = std::chrono::system_clock::to_time_t(now);

    std::strftime(s_SystemTimeMessageBuffer, s_MaxSystemTimeMessageLength, "[%d/%m/%Y][%X]", std::localtime(&rawTime));
    return s_SystemTimeMessageBuffer;
}

}  // namespace Pathfinder