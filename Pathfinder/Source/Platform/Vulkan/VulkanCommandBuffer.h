#ifndef VULKANCOMMANDBUFFER_H
#define VULKANCOMMANDBUFFER_H

#include "Renderer/CommandBuffer.h"
#include "VulkanCore.h"

namespace Pathfinder
{
class VulkanPipeline;
class Pipeline;

class VulkanCommandBuffer final : public CommandBuffer
{
  public:
    explicit VulkanCommandBuffer(ECommandBufferType type, ECommandBufferLevel level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY);
    VulkanCommandBuffer() = delete;
    ~VulkanCommandBuffer() override { Destroy(); }

    NODISCARD FORCEINLINE ECommandBufferType GetType() const final override { return m_Type; }
    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE void* GetWaitSemaphore() const final override { return m_WaitSemaphore; }
    NODISCARD FORCEINLINE ECommandBufferLevel GetLevel() const final override { return m_Level; }
    const std::vector<float> GetTimestampsResults() const final override;

    void BeginPipelineStatisticsQuery() final override;
    FORCEINLINE void EndPipelineStatisticsQuery() const final override { vkCmdEndQuery(m_Handle, m_PipelineStatisticsQuery, 0); }

    void BeginTimestampQuery(const EPipelineStage pipelineStage) final override;
    FORCEINLINE void EndTimestampQuery(const EPipelineStage pipelineStage) final override
    {
        PFR_ASSERT(m_CurrentTimestampIndex + 1 < s_MAX_TIMESTAMPS, "Timestamp query limit reached!");
        vkCmdWriteTimestamp(m_Handle, (VkPipelineStageFlagBits)VulkanUtility::PathfinderPipelineStageToVulkan(pipelineStage),
                            m_TimestampQuery, m_CurrentTimestampIndex);
        ++m_CurrentTimestampIndex;
    }

    void BeginDebugLabel(std::string_view label, const glm::vec3& color = glm::vec3(1.0f)) const final override
    {
        if constexpr (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
        {
            VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
            labelInfo.pLabelName           = label.data();
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

    FORCEINLINE void Reset() final override { VK_CHECK(vkResetCommandBuffer(m_Handle, 0), "Failed to reset command buffer!"); }

    void BeginRecording(bool bOneTimeSubmit = false, const void* inheritanceInfo = VK_NULL_HANDLE) final override;
    FORCEINLINE void EndRecording() final override { VK_CHECK(vkEndCommandBuffer(m_Handle), "Failed to end recording command buffer"); }

    void Submit(bool bWaitAfterSubmit = true, bool bSignalWaitSemaphore = false, const PipelineStageFlags pipelineStages = 0,
                const std::vector<void*>& semaphoresToWaitOn = {}) final override;
    void TransitionImageLayout(const VkImage& image, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                               const VkImageAspectFlags aspectMask) const;

    FORCEINLINE void BeginRendering(const VkRenderingInfo* renderingInfo) const { vkCmdBeginRendering(m_Handle, renderingInfo); }
    FORCEINLINE void EndRendering() const { vkCmdEndRendering(m_Handle); }

    FORCEINLINE void InsertBarrier(const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                                   const VkDependencyFlags dependencyFlags, const uint32_t memoryBarrierCount,
                                   const VkMemoryBarrier* pMemoryBarriers, const uint32_t bufferMemoryBarrierCount,
                                   const VkBufferMemoryBarrier* pBufferMemoryBarriers, const uint32_t imageMemoryBarrierCount,
                                   const VkImageMemoryBarrier* pImageMemoryBarriers) const
    {
        vkCmdPipelineBarrier(m_Handle, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers,
                             bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    void BindShaderData(Shared<Pipeline>& pipeline, const Shared<Shader>& shader) const final override;
    void BindPushConstants(Shared<Pipeline>& pipeline, const uint32_t pushConstantIndex, const uint32_t offset, const uint32_t size,
                           const void* data = nullptr) const final override;
    void BindDescriptorSets(Shared<VulkanPipeline>& pipeline, const uint32_t firstSet = 0, const uint32_t descriptorSetCount = 0,
                            VkDescriptorSet* descriptorSets = VK_NULL_HANDLE, const uint32_t dynamicOffsetCount = 0,
                            uint32_t* dynamicOffsets = nullptr) const;

    void BindPipeline(Shared<Pipeline>& pipeline) const final override;

    FORCEINLINE void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                 const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const final override
    {
        vkCmdDrawIndexed(m_Handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    FORCEINLINE void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                          const uint32_t firstInstance = 0) const final override
    {
        vkCmdDraw(m_Handle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding = 0,
                           const uint32_t bindingCount = 1, const uint64_t* offsets = nullptr) const final override;

    // NOTE: Idk why would I need this for now, but let it be
    FORCEINLINE void BindVertexBuffers(const uint32_t firstBinding = 0, const uint32_t bindingCount = 1, VkBuffer* buffers = VK_NULL_HANDLE,
                                       VkDeviceSize* offsets = VK_NULL_HANDLE) const
    {
        PFR_ASSERT(buffers, "Invalid vertex buffer(s)!");
        vkCmdBindVertexBuffers(m_Handle, firstBinding, bindingCount, buffers, offsets);
    }

    void BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset = 0, bool bIndexType32 = true) const final override;

    FORCEINLINE void DrawMeshTasks(const uint32_t groupCountX, const uint32_t groupCountY,
                                   const uint32_t groupCountZ = 1) const final override
    {
        vkCmdDrawMeshTasksEXT(m_Handle, groupCountX, groupCountY, groupCountZ);
    }

    // COMPUTE

    FORCEINLINE void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ) final override
    {
        vkCmdDispatch(m_Handle, groupCountX, groupCountY, groupCountZ);
    }

    // RT
    FORCEINLINE void TraceRays(const VkStridedDeviceAddressRegionKHR* raygenShaderBindingTable,
                               const VkStridedDeviceAddressRegionKHR* missShaderBindingTable,
                               const VkStridedDeviceAddressRegionKHR* hitShaderBindingTable,
                               const VkStridedDeviceAddressRegionKHR* callableShaderBindingTable, uint32_t width, uint32_t height,
                               uint32_t depth = 1) const
    {
        vkCmdTraceRaysKHR(m_Handle, raygenShaderBindingTable, missShaderBindingTable, hitShaderBindingTable, callableShaderBindingTable,
                          width, height, depth);
    }

    FORCEINLINE void BuildAccelerationStructure(uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* infos,
                                                const VkAccelerationStructureBuildRangeInfoKHR* const* buildRangeInfos) const
    {
        vkCmdBuildAccelerationStructuresKHR(m_Handle, infoCount, infos, buildRangeInfos);
    }

    // TRANSFER
    FORCEINLINE void CopyBuffer(const VkBuffer& srcBuffer, VkBuffer& dstBuffer, const uint32_t regionCount, const VkBufferCopy* pRegions)
    {
        vkCmdCopyBuffer(m_Handle, srcBuffer, dstBuffer, regionCount, pRegions);
    }

    FORCEINLINE void CopyBufferToImage(const VkBuffer& srcBuffer, VkImage& dstImage, const VkImageLayout dstImageLayout,
                                       const uint32_t regionCount, const VkBufferImageCopy* pRegions)
    {
        vkCmdCopyBufferToImage(m_Handle, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }

    void CopyImageToImage(const Shared<Image> srcImage, Shared<Image> dstImage) const final override;

    FORCEINLINE void BlitImage(const VkImage& srcImage, const VkImageLayout srcImageLayout, VkImage& dstImage,
                               const VkImageLayout dstImageLayout, const uint32_t regionCount, const VkImageBlit* pRegions,
                               const VkFilter filter)
    {
        vkCmdBlitImage(m_Handle, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }

  private:
    VkCommandBuffer m_Handle    = VK_NULL_HANDLE;
    VkFence m_SubmitFence       = VK_NULL_HANDLE;
    VkSemaphore m_WaitSemaphore = VK_NULL_HANDLE;

    VkQueryPool m_PipelineStatisticsQuery = VK_NULL_HANDLE;

    VkQueryPool m_TimestampQuery     = VK_NULL_HANDLE;
    uint32_t m_CurrentTimestampIndex = 0;

    ECommandBufferLevel m_Level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY;
    ECommandBufferType m_Type   = ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS;

    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANCOMMANDBUFFER_H
