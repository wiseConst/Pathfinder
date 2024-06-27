#pragma once

#include <Core/Core.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/GraphicsContext.h>

namespace Pathfinder
{

class GPUProfiler final
{
  public:
    GPUProfiler() = default;
    explicit GPUProfiler(const uint32_t queryCount)
        : m_ProfilerQueryPool(QueryPool::Create(queryCount * 2 /* Doubling for begin and end timestamps.*/)),
          m_PipelineStatisticsQueryPool(QueryPool::Create(1, true))
    {
    }
    ~GPUProfiler() = default;

    FORCEINLINE void BeginFrame()
    {
        m_ProfilerTasks.clear();
        m_PipelineStatistics.clear();
        m_CurrentTimestampIndex = 0;
    }
    FORCEINLINE void EndFrame() {}

    FORCEINLINE void BeginPipelineStatisticsQuery(const Shared<CommandBuffer>& commandBuffer)
    {
        commandBuffer->ResetPool(m_PipelineStatisticsQueryPool);
        commandBuffer->BeginPipelineStatisticsQuery(m_PipelineStatisticsQueryPool);
    }

    FORCEINLINE void EndPipelineStatisticsQuery(const Shared<CommandBuffer>& commandBuffer)
    {
        commandBuffer->EndPipelineStatisticsQuery(m_PipelineStatisticsQueryPool);
    }

    FORCEINLINE void BeginTimestamp(const Shared<CommandBuffer>& commandBuffer, const std::string& name, const glm::vec3& color,
                                    const EPipelineStage pipelineStage = EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT)
    {
        if (m_ProfilerTasks.empty()) commandBuffer->ResetPool(m_ProfilerQueryPool);

        auto& task = m_ProfilerTasks.emplace_back();
        task.Tag   = name;
        task.Color = color;

        commandBuffer->WriteTimestamp(m_ProfilerQueryPool, m_CurrentTimestampIndex++, pipelineStage);
    }

    FORCEINLINE void EndTimestamp(const Shared<CommandBuffer>& commandBuffer,
                                  const EPipelineStage pipelineStage = EPipelineStage::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
    {
        commandBuffer->WriteTimestamp(m_ProfilerQueryPool, m_CurrentTimestampIndex++, pipelineStage);
    }

    FORCEINLINE void CalculateResults(const Shared<CommandBuffer>& commandBuffer)
    {
        m_PipelineStatistics          = commandBuffer->CalculateQueryPoolStatisticsResults(m_PipelineStatisticsQueryPool);
        const auto rawProfilerResults = commandBuffer->CalculateQueryPoolProfilerResults(m_ProfilerQueryPool, m_ProfilerTasks.size() * 2);

        // NOTE: timestampPeriod contains the number of nanoseconds it takes for a timestamp query value to be increased by 1("tick").
        const float timestampPeriod = GraphicsContext::Get().GetTimestampPeriod();
        for (size_t i{}, k{}; i < rawProfilerResults.size() - 1; i += 2, ++k)
        {
            auto& task     = m_ProfilerTasks.at(k);
            task.StartTime = 0.f;
            task.EndTime   = static_cast<float>(rawProfilerResults[i + 1] - rawProfilerResults[i]) * timestampPeriod /
                           1000000.0f;  // convert nanoseconds to milliseconds
        }
    }
    NODISCARD FORCEINLINE const auto& GetProfilerResults() const { return m_ProfilerTasks; }
    NODISCARD FORCEINLINE const auto& GetPipelineStatisticsResults() const { return m_PipelineStatistics; }

  private:
    Shared<QueryPool> m_PipelineStatisticsQueryPool = nullptr;
    Shared<QueryPool> m_ProfilerQueryPool           = nullptr;
    uint32_t m_CurrentTimestampIndex                = 0;
    std::vector<ProfilerTask> m_ProfilerTasks;
    std::vector<std::pair<std::string, std::uint64_t>> m_PipelineStatistics;
};

}  // namespace Pathfinder