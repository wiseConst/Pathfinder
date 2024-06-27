#include <PathfinderPCH.h>
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "Application.h"

namespace Pathfinder
{

void Log::Init(const std::string_view& logFileName)
{
    assert(logFileName.data());

    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    constexpr bool bTruncate = true;
    logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName.data(), bTruncate));

    logSinks[0]->set_pattern("%^[Thread: %t][%T] %n: %v%$");
    logSinks[1]->set_pattern("[Thread: %t][%T] [%l] %n: %v");

    s_Logger = std::make_shared<spdlog::logger>(std::string(s_ENGINE_NAME), begin(logSinks), end(logSinks));
    spdlog::register_logger(s_Logger);
    s_Logger->set_level(spdlog::level::trace);
    s_Logger->flush_on(spdlog::level::trace);
}

}  // namespace Pathfinder