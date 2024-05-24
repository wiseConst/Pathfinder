#pragma once

#include "Core.h"
#include <chrono>

namespace Pathfinder
{

class Timer final
{
  public:
    Timer() noexcept = default;
    ~Timer()         = default;

    FORCEINLINE double GetElapsedSeconds() const { return GetElapsedMilliseconds() / 1000; }

    FORCEINLINE double GetElapsedMilliseconds() const
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_StartTime);
        return elapsed.count();
    }

    FORCEINLINE static std::chrono::time_point<std::chrono::high_resolution_clock> Now()
    {
        return std::chrono::high_resolution_clock::now();
    }

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTime = Now();
};

}  // namespace Pathfinder
