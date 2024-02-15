#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Core/Core.h"
#include "Core/Math.h"
#include "Image.h"
#include "RendererCoreDefines.h"
#include "Shader.h"

namespace Pathfinder
{

enum class ECommandBufferType : uint8_t
{
    COMMAND_BUFFER_TYPE_GRAPHICS = 0,
    COMMAND_BUFFER_TYPE_COMPUTE,
    COMMAND_BUFFER_TYPE_TRANSFER
};

enum class ECommandBufferLevel : uint8_t
{
    COMMAND_BUFFER_LEVEL_PRIMARY   = 0,
    COMMAND_BUFFER_LEVEL_SECONDARY = 1
};

class Pipeline;
class CommandBuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~CommandBuffer() = default;

    NODISCARD FORCEINLINE virtual ECommandBufferType GetType() const   = 0;
    NODISCARD FORCEINLINE virtual ECommandBufferLevel GetLevel() const = 0;
    NODISCARD FORCEINLINE virtual void* Get() const                    = 0;
    NODISCARD FORCEINLINE virtual void* GetWaitSemaphore() const       = 0;

    NODISCARD FORCEINLINE static const auto& GetPipelineStatisticsNames() { return s_PipelineStatisticsNames; }
    NODISCARD FORCEINLINE const auto& GetPipelineStatisticsResults() const { return m_PipelineStatisticsResults; }
    NODISCARD FORCEINLINE static const char* ConvertQueryPipelineStatisticToCString(const EQueryPipelineStatistic queryPipelineStatistic)
    {
        return s_PipelineStatisticsNames[queryPipelineStatistic].data();
    }

    // Returns raw vector of ticks.
    NODISCARD FORCEINLINE const auto& GetRawTimestampsResults() const { return m_TimestampsResults; }

    // Returns vector of computed timestamps in milliseconds.
    virtual const std::vector<float> GetTimestampsResults() const = 0;

    /* STATISTICS  */
    virtual void BeginPipelineStatisticsQuery()     = 0;
    virtual void EndPipelineStatisticsQuery() const = 0;

    virtual void BeginTimestampQuery(const EPipelineStage pipelineStage = EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT)  = 0;
    virtual void EndTimestampQuery(const EPipelineStage pipelineStage = EPipelineStage::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) = 0;
    /* STATISTICS  */

    /* DEBUG */
    virtual void BeginDebugLabel(std::string_view label, const glm::vec3& color = glm::vec3(1.0f)) const = 0;
    virtual void EndDebugLabel() const                                                                   = 0;
    /* DEBUG */

    virtual void BeginRecording(bool bOneTimeSubmit = false, const void* inheritanceInfo = nullptr) = 0;
    virtual void EndRecording()                                                                     = 0;

    virtual void BindPipeline(Shared<Pipeline>& pipeline) const = 0;

    virtual void BindShaderData(Shared<Pipeline>& pipeline, const Shared<Shader>& shader) const = 0;
    virtual void BindPushConstants(Shared<Pipeline>& pipeline, const uint32_t pushConstantIndex, const uint32_t offset, const uint32_t size,
                                   const void* data = nullptr) const                            = 0;

    FORCEINLINE virtual void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ = 1) = 0;

    FORCEINLINE virtual void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                         const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                                  const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void DrawMeshTasks(const uint32_t groupCountX, const uint32_t groupCountY,
                                           const uint32_t groupCountZ = 1) const = 0;

    virtual void BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding = 0,
                                   const uint32_t bindingCount = 1, const uint64_t* offsets = nullptr) const                   = 0;
    virtual void BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset = 0, bool bIndexType32 = true) const = 0;

    virtual void CopyImageToImage(const Shared<Image> srcImage, Shared<Image> dstImage) const                               = 0;
    virtual void InsertExecutionBarrier(const EPipelineStage srcPipelineStage, const EPipelineStage dstPipelineStage) const = 0;

    virtual void WaitForSubmitFence()                                     = 0;
    virtual void Submit(bool bWaitAfterSubmit = true, bool bSignalWaitSemaphore = false, const PipelineStageFlags pipelineStages = 0,
                        const std::vector<void*>& semaphoresToWaitOn = {}, const uint32_t waitSemaphoreValueCount = 0,
                        const uint64_t* pWaitSemaphoreValues = nullptr, const uint32_t signalSemaphoreValueCount = 0,
                        const uint64_t* pSignalSemaphoreValues = nullptr) = 0;
    virtual void Reset()                                                  = 0;

    static Shared<CommandBuffer> Create(ECommandBufferType type,
                                        ECommandBufferLevel level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY);

  protected:
    static constexpr uint32_t s_MAX_TIMESTAMPS          = 32;
    static constexpr uint32_t s_MAX_PIPELINE_STATISITCS = 13;

    static inline std::map<const EQueryPipelineStatistic, std::string_view> s_PipelineStatisticsNames = {
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
         "Input assembly vertex count        "},  // IA vertex
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
         "Input assembly primitives count    "},  // IA primitives
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT, "Vertex shader invocations          "},    // VS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT, "Geometry shader invocations        "},  // GS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT, "Geometry shader primitives         "},   //
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT, "Clipping stage primitives processed"},  // Clipping
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT, "Clipping stage primitives output   "},   //
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT, "Fragment shader invocations        "},  // FS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT,
         "Tess. Control shader invocations   "},  // TCS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT,
         "Tess. Evaluation shader invocations"},                                                                                    // TES
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT, "Compute shader invocations         "},  // CSCS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT, "Task shader invocations            "},     // TS
        {EQueryPipelineStatistic::QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT, "Mesh shader invocations            "}      // MS
    };

    std::array<std::pair<EQueryPipelineStatistic, uint64_t>, s_MAX_PIPELINE_STATISITCS> m_PipelineStatisticsResults;
    std::array<uint64_t, s_MAX_TIMESTAMPS> m_TimestampsResults = {0};

    CommandBuffer()        = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // COMMANDBUFFER_H
