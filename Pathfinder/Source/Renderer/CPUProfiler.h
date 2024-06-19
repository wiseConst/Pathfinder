#pragma once

#include <Core/Core.h>

namespace Pathfinder
{

class CPUProfiler final
{
  public:
    CPUProfiler()  = default;
    ~CPUProfiler() = default;

    FORCEINLINE void BeginTimestamp(const std::string& name) { m_Stats.emplace_back(name, 0.); }
    FORCEINLINE void EndTimestamp()
    {
        auto& lastEntry = m_Stats.back();
        //  lastEntry.TimeMs = Timer::Now() - lastEntry.TimeMs;
    }

    NODISCARD FORCEINLINE const auto& GetResults() const { return m_Stats; }

  private:
    struct ProfilerEntry
    {
        std::string Name = s_DEFAULT_STRING;
        double TimeMs    = 0.;
    };

    std::vector<ProfilerEntry> m_Stats;
};

}  // namespace Pathfinder