#pragma once

#include <Core/Core.h>
#include <Core/Math.h>
#include "Image.h"
#include "RendererCoreDefines.h"
#include "Shader.h"

namespace Pathfinder
{

struct MemoryBarrier
{
    RendererTypeFlags srcStageMask  = EPipelineStage::PIPELINE_STAGE_NONE;
    RendererTypeFlags srcAccessMask = EAccessFlags::ACCESS_NONE;
    RendererTypeFlags dstStageMask  = EPipelineStage::PIPELINE_STAGE_NONE;
    RendererTypeFlags dstAccessMask = EAccessFlags::ACCESS_NONE;
};

struct BufferMemoryBarrier
{
    Shared<Buffer> buffer{};
    RendererTypeFlags srcStageMask{};
    RendererTypeFlags srcAccessMask{};
    RendererTypeFlags dstStageMask{};
    RendererTypeFlags dstAccessMask{};
    uint32_t srcQueueFamilyIndex = UINT32_MAX;
    uint32_t dstQueueFamilyIndex = UINT32_MAX;
};

struct ImageMemoryBarrier
{
    RendererTypeFlags srcStageMask{};
    RendererTypeFlags srcAccessMask{};
    RendererTypeFlags dstStageMask{};
    RendererTypeFlags dstAccessMask{};
    EImageLayout oldLayout{};
    EImageLayout newLayout{};
    uint32_t srcQueueFamilyIndex = UINT32_MAX;
    uint32_t dstQueueFamilyIndex = UINT32_MAX;
    Shared<Image> image{};

    struct
    {
        uint32_t baseMipLevel;
        uint32_t levelCount;
        uint32_t baseArrayLayer;
        uint32_t layerCount;
    } subresourceRange;
};

enum class ECommandBufferType : uint8_t
{
    COMMAND_BUFFER_TYPE_GRAPHICS = 0,
    COMMAND_BUFFER_TYPE_COMPUTE,
    COMMAND_BUFFER_TYPE_TRANSFER
};

enum class ECommandBufferLevel : uint8_t
{
    COMMAND_BUFFER_LEVEL_PRIMARY = 0,
    COMMAND_BUFFER_LEVEL_SECONDARY
};

struct CommandBufferSpecification
{
    ECommandBufferType Type   = ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS;
    ECommandBufferLevel Level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY;
    uint8_t FrameIndex        = 0;
    uint8_t ThreadID          = 0;
};

class SyncPoint : private Uncopyable, private Unmovable
{
  public:
    SyncPoint(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages) noexcept
        : m_TimelineSemaphoreHandle(timelineSemaphoreHandle), m_Value(value), m_PipelineStages(pipelineStages)
    {
    }
    virtual ~SyncPoint() = default;

    NODISCARD FORCEINLINE const auto& GetValue() const { return m_Value; }
    NODISCARD FORCEINLINE const auto& GetTimelineSemaphore() const { return m_TimelineSemaphoreHandle; }
    NODISCARD FORCEINLINE const auto& GetPipelineStages() const { return m_PipelineStages; }

    virtual void Wait() const = 0;  // Waits for the semaphore to reach 'value'
    static Shared<SyncPoint> Create(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages);

  protected:
    void* m_TimelineSemaphoreHandle    = nullptr;
    uint64_t m_Value                   = 0;
    RendererTypeFlags m_PipelineStages = EPipelineStage::PIPELINE_STAGE_NONE;

  private:
    SyncPoint() = delete;
};

class Pipeline;
class Texture;
class CommandBuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~CommandBuffer() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;

    /* DEBUG */
    FORCEINLINE void InsertDebugBarrier() const
    {
        if constexpr (PFR_DEBUG)
        {
            InsertBarriers({{.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             .srcAccessMask = EAccessFlags::ACCESS_MEMORY_READ_BIT | EAccessFlags::ACCESS_MEMORY_WRITE_BIT,
                             .dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             .dstAccessMask = EAccessFlags::ACCESS_MEMORY_READ_BIT | EAccessFlags::ACCESS_MEMORY_WRITE_BIT}});
        }
    }
    virtual void BeginDebugLabel(std::string_view label, const glm::vec3& color = glm::vec3(1.0f)) const = 0;
    virtual void EndDebugLabel() const                                                                   = 0;
    /* DEBUG */

    virtual void BeginRecording(bool bOneTimeSubmit = false, const void* inheritanceInfo = nullptr) = 0;
    virtual void EndRecording() const                                                               = 0;

    virtual void SetViewportAndScissor(const uint32_t width, const uint32_t height, const int32_t offsetX = 0,
                                       const int32_t offsetY = 0) const                 = 0;
    virtual void BindPipeline(Shared<Pipeline>& pipeline) const                         = 0;
    virtual void BindPushConstants(Shared<Pipeline> pipeline, const uint32_t offset, const uint32_t size,
                                   const void* data = nullptr) const                    = 0;
    virtual void BeginRendering(const std::vector<Shared<Texture>>& attachments,
                                const std::vector<RenderingInfo>& renderingInfos) const = 0;
    virtual void EndRendering() const                                                   = 0;

    FORCEINLINE virtual void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY = 1, const uint32_t groupCountZ = 1) const = 0;

    FORCEINLINE virtual void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                         const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void DrawIndexedIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                                 const uint32_t stride) const = 0;

    FORCEINLINE virtual void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                                  const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void DrawMeshTasks(const uint32_t groupCountX, const uint32_t groupCountY = 1,
                                           const uint32_t groupCountZ = 1) const = 0;

    FORCEINLINE virtual void DrawMeshTasksIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                                   const uint32_t stride) const = 0;

    FORCEINLINE virtual void DrawMeshTasksMultiIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset,
                                                        const Shared<Buffer>& countBuffer, const uint64_t countBufferOffset,
                                                        const uint32_t maxDrawCount, const uint32_t stride) const = 0;

    virtual void TraceRays(const ShaderBindingTable& sbt, uint32_t width, uint32_t height, uint32_t depth = 1) const = 0;

    virtual void BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding = 0,
                                   const uint32_t bindingCount = 1, const uint64_t* offsets = nullptr) const                   = 0;
    virtual void BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset = 0, bool bIndexType32 = true) const = 0;

    virtual void FillBuffer(const Shared<Buffer>& buffer, const uint32_t data) const = 0;

    virtual void InsertBarriers(const std::vector<MemoryBarrier>& memoryBarriers,
                                const std::vector<BufferMemoryBarrier>& bufferMemoryBarriers = {},
                                const std::vector<ImageMemoryBarrier>& imageMemoryBarriers   = {}) const = 0;

    virtual Shared<SyncPoint> Submit(const std::vector<Shared<SyncPoint>>& waitPoints   = {},
                                     const std::vector<Shared<SyncPoint>>& signalPoints = {}, const void* signalFence = nullptr) = 0;

    static Shared<CommandBuffer> Create(const CommandBufferSpecification& commandBufferSpec);

  protected:
    CommandBufferSpecification m_Specification = {};

    CommandBuffer(const CommandBufferSpecification& commandBufferSpec) : m_Specification(commandBufferSpec) {}
    CommandBuffer()        = delete;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder
