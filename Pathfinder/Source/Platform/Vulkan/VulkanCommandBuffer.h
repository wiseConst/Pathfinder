#pragma once

#include "Renderer/CommandBuffer.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanQueryPool : public QueryPool
{
  public:
    VulkanQueryPool(const uint32_t queryCount, const bool bIsPipelineStatistics);
    ~VulkanQueryPool() { Destroy(); };

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }

  private:
    VkQueryPool m_Handle = VK_NULL_HANDLE;

    void Destroy() final override;
};

class VulkanSyncPoint final : public SyncPoint
{
  public:
    VulkanSyncPoint(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages) noexcept;
    ~VulkanSyncPoint() noexcept override = default;

    void Wait() const noexcept final override;

  private:
    VulkanSyncPoint() = delete;
};

class VulkanPipeline;
class Pipeline;
class Buffer;

class VulkanCommandBuffer final : public CommandBuffer
{
  public:
    explicit VulkanCommandBuffer(const CommandBufferSpecification& commandBufferSpec);
    ~VulkanCommandBuffer() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }

    FORCEINLINE void BeginDebugLabel(std::string_view label, const glm::vec3& color = glm::vec3(1.0f)) const final override
    {
        if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
        {
            VkDebugUtilsLabelEXT labelInfo = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = label.data()};
            labelInfo.color[0]             = color.r;
            labelInfo.color[1]             = color.g;
            labelInfo.color[2]             = color.b;
            labelInfo.color[3]             = 1.0f;
            vkCmdBeginDebugUtilsLabelEXT(m_Handle, &labelInfo);
        }
    }

    FORCEINLINE void EndDebugLabel() const final override
    {
        if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers) vkCmdEndDebugUtilsLabelEXT(m_Handle);
    }

    void FillBuffer(const Shared<Buffer>& buffer, const uint32_t data) const final override;

    void InsertBarriers(const std::vector<MemoryBarrier>& memoryBarriers, const std::vector<BufferMemoryBarrier>& bufferMemoryBarriers = {},
                        const std::vector<ImageMemoryBarrier>& imageMemoryBarriers = {}) const final override;

    void BeginPipelineStatisticsQuery(Shared<QueryPool>& queryPool) final override;
    void EndPipelineStatisticsQuery(Shared<QueryPool>& queryPool) final override;

    std::vector<std::pair<std::string, std::uint64_t>> CalculateQueryPoolStatisticsResults(Shared<QueryPool>& queryPool) final override;
    std::vector<uint64_t> CalculateQueryPoolProfilerResults(Shared<QueryPool>& queryPool, const size_t timestampCount) final override;
    void ResetPool(Shared<QueryPool>& queryPool) final override;
    void WriteTimestamp(Shared<QueryPool>& queryPool, const uint32_t timestampIndex,
                        const EPipelineStage pipelineStage = EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT) final override;

    void BeginRecording(bool bOneTimeSubmit = false, const void* inheritanceInfo = VK_NULL_HANDLE) final override;
    void EndRecording() const final override;

    Shared<SyncPoint> Submit(const std::vector<Shared<SyncPoint>>& waitPoints = {}, const std::vector<Shared<SyncPoint>>& signalPoints = {},
                             const void* signalFence = nullptr) final override;
    void TransitionImageLayout(const VkImage& image, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                               const VkImageAspectFlags aspectMask, const uint32_t layerCount, const uint32_t baseLayer,
                               const uint32_t mipLevels, const uint32_t baseMipLevel) const;

    void BeginRendering(const std::vector<Shared<Texture>>& attachments,
                        const std::vector<RenderingInfo>& renderingInfos) const final override;
    FORCEINLINE void BeginRendering(const VkRenderingInfo* renderingInfo) const { vkCmdBeginRendering(m_Handle, renderingInfo); }
    FORCEINLINE void EndRendering() const final override { vkCmdEndRendering(m_Handle); }

    void InsertBarrier(const VkPipelineStageFlags2 srcStageMask, const VkPipelineStageFlags2 dstStageMask,
                       const VkDependencyFlags dependencyFlags, const uint32_t memoryBarrierCount, const VkMemoryBarrier2* pMemoryBarriers,
                       const uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier2* pBufferMemoryBarriers,
                       const uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier2* pImageMemoryBarriers) const;

    void BindPushConstants(Shared<Pipeline> pipeline, const uint32_t offset, const uint32_t size, const void* data = nullptr) const;
    void SetViewportAndScissor(const uint32_t width, const uint32_t height, const int32_t offsetX = 0,
                               const int32_t offsetY = 0) const final override;
    void BindPipeline(Shared<Pipeline>& pipeline) const final override;

    FORCEINLINE void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                 const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const final override
    {
        vkCmdDrawIndexed(m_Handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    FORCEINLINE void DrawIndexedIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                         const uint32_t stride) const final override;

    FORCEINLINE void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                          const uint32_t firstInstance = 0) const final override
    {
        vkCmdDraw(m_Handle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding = 0,
                           const uint32_t bindingCount = 1, const uint64_t* offsets = nullptr) const final override;

    void BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset = 0, bool bIndexType32 = true) const final override;

    FORCEINLINE void DrawMeshTasks(const uint32_t groupCountX, const uint32_t groupCountY,
                                   const uint32_t groupCountZ = 1) const final override
    {
        vkCmdDrawMeshTasksEXT(m_Handle, groupCountX, groupCountY, groupCountZ);
    }

    FORCEINLINE void DrawMeshTasksIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                           const uint32_t stride) const final override;

    FORCEINLINE void DrawMeshTasksMultiIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const Shared<Buffer>& countBuffer,
                                                const uint64_t countBufferOffset, const uint32_t maxDrawCount,
                                                const uint32_t stride) const final override;

    // COMPUTE
    FORCEINLINE void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ) const final override
    {
        vkCmdDispatch(m_Handle, groupCountX, groupCountY, groupCountZ);
    }

    // RT
    void TraceRays(const ShaderBindingTable& sbt, uint32_t width, uint32_t height, uint32_t depth = 1) const final override
    {
        const VkStridedDeviceAddressRegionKHR* rgenRegion     = (VkStridedDeviceAddressRegionKHR*)&sbt.RgenRegion;
        const VkStridedDeviceAddressRegionKHR* missRegion     = (VkStridedDeviceAddressRegionKHR*)&sbt.MissRegion;
        const VkStridedDeviceAddressRegionKHR* hitRegion      = (VkStridedDeviceAddressRegionKHR*)&sbt.HitRegion;
        const VkStridedDeviceAddressRegionKHR* callableRegion = (VkStridedDeviceAddressRegionKHR*)&sbt.CallRegion;

        vkCmdTraceRaysKHR(m_Handle, rgenRegion, missRegion, hitRegion, callableRegion, width, height, depth);
    }

    FORCEINLINE void BuildAccelerationStructure(uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* infos,
                                                const VkAccelerationStructureBuildRangeInfoKHR* const* buildRangeInfos) const
    {
        vkCmdBuildAccelerationStructuresKHR(m_Handle, infoCount, infos, buildRangeInfos);
    }

    // TRANSFER
    FORCEINLINE void CopyBuffer(const VkBuffer& srcBuffer, VkBuffer& dstBuffer, const uint32_t regionCount,
                                const VkBufferCopy* pRegions) const
    {
        vkCmdCopyBuffer(m_Handle, srcBuffer, dstBuffer, regionCount, pRegions);
    }

    FORCEINLINE void CopyBufferToImage(const VkBuffer& srcBuffer, VkImage& dstImage, const VkImageLayout dstImageLayout,
                                       const uint32_t regionCount, const VkBufferImageCopy* pRegions) const
    {
        vkCmdCopyBufferToImage(m_Handle, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }

    FORCEINLINE void BlitImage(const VkImage& srcImage, const VkImageLayout srcImageLayout, VkImage& dstImage,
                               const VkImageLayout dstImageLayout, const uint32_t regionCount, const VkImageBlit* pRegions,
                               const VkFilter filter) const
    {
        vkCmdBlitImage(m_Handle, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }

  private:
    VkCommandBuffer m_Handle = VK_NULL_HANDLE;
    struct
    {
        VkSemaphore Handle = VK_NULL_HANDLE;
        uint64_t Counter   = 0;
    } m_TimelineSemaphore;

    void Destroy() final override;
    VulkanCommandBuffer() = delete;
};

}  // namespace Pathfinder
