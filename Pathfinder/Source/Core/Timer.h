#ifndef TIMER_H
#define TIMER_H

#include "Core.h"
#include <chrono>

namespace Pathfinder
{

class Timer final
{
  public:
    Timer() noexcept = default;
    ~Timer()         = default;

    FORCEINLINE double GetElapsedMilliseconds() const
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_StartTime);
        return elapsed.count();
    }

    FORCEINLINE static auto Now() { return std::chrono::high_resolution_clock::now(); }

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTime = std::chrono::high_resolution_clock::now();
};

}  // namespace Pathfinder

#endif  // TIMER_H
