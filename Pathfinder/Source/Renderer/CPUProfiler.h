#pragma once

#include <Core/Core.h>

namespace Pathfinder
{

class CPUProfiler final
{
  public:
    CPUProfiler()  = default;
    ~CPUProfiler() = default;

    FORCEINLINE void BeginFrame()
    {
        m_ProfilerTasks.clear();
        m_FrameStartTime = Timer::Now();
        ++m_FrameNumber;
    }
    FORCEINLINE void EndFrame() {}

    FORCEINLINE void BeginTimestamp(const std::string& name, const glm::vec3& color = glm::vec3{1.f})
    {
        m_ProfilerTasks.emplace_back(GetCurrentFrameTimeSeconds(), 0.0, name, color);
    }

    FORCEINLINE void EndTimestamp() { m_ProfilerTasks.back().EndTime = GetCurrentFrameTimeSeconds(); }

    NODISCARD FORCEINLINE const auto& GetResults() const { return m_ProfilerTasks; }

  private:
    std::chrono::high_resolution_clock::time_point m_FrameStartTime{};
    std::vector<ProfilerTask> m_ProfilerTasks;
    uint64_t m_FrameNumber{};

    NODISCARD FORCEINLINE double GetCurrentFrameTimeSeconds() const
    {
        return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(Timer::Now() - m_FrameStartTime).count());
    }
};

}  // namespace Pathfinder