#pragma once

#include "MathDefines.h"
#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Pathfinder
{

class Log final
{
  public:
    static void Init(const std::string_view& logFileName);
    static void Shutdown() { spdlog::shutdown(); }

    static auto& GetLogger() { return s_Logger; }

  private:
    static inline std::shared_ptr<spdlog::logger> s_Logger = nullptr;

    Log()  = delete;
    ~Log() = default;
};

}  // namespace Pathfinder

#define LOG_TRACE(...) ::Pathfinder::Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...) ::Pathfinder::Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::Pathfinder::Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Pathfinder::Log::GetLogger()->error(__VA_ARGS__)

#if PFR_DEBUG
#define LOG_DEBUG(...) ::Pathfinder::Log::GetLogger()->debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

// TODO: Fix, cuz it doesn't work rn.
namespace std
{

template <typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector)
{
    return os << glm::to_string(vector);
}

template <typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix)
{
    return os << glm::to_string(matrix);
}

template <typename OStream, typename T, glm::qualifier Q> inline OStream& operator<<(OStream& os, glm::qua<T, Q> quaternion)
{
    return os << glm::to_string(quaternion);
}

}  // namespace std