#ifndef VULKANCOMMANDBUFFER_H
#define VULKANCOMMANDBUFFER_H

#include "Renderer/CommandBuffer.h"
#include "VulkanCore.h"

namespace Pathfinder
{
class VulkanPipeline;

class VulkanCommandBuffer final : public CommandBuffer
{
  public:
    explicit VulkanCommandBuffer(ECommandBufferType type, ECommandBufferLevel level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY);
    VulkanCommandBuffer() = delete;
    ~VulkanCommandBuffer() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE ECommandBufferLevel GetLevel() const final override { return m_Level; }

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

    void Submit(bool bWaitAfterSubmit = true) final override;
    void TransitionImageLayout(const Shared<Image>& image, const EImageLayout newLayout, const EPipelineStage srcPipelineStage,
                               const EPipelineStage dstPipelineStage) final override;

    FORCEINLINE void CopyImageToImage(const VkImage& srcImage, const VkImageLayout srcImageLayout, VkImage& dstImage,
                                      const VkImageLayout dstImageLayout, const uint32_t regionCount, const VkImageCopy* regions) const
    {
        vkCmdCopyImage(m_Handle, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, regions);
    }

    FORCEINLINE void InsertBarrier(const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                                   const VkDependencyFlags dependencyFlags, const uint32_t memoryBarrierCount,
                                   const VkMemoryBarrier* pMemoryBarriers, const uint32_t bufferMemoryBarrierCount,
                                   const VkBufferMemoryBarrier* pBufferMemoryBarriers, const uint32_t imageMemoryBarrierCount,
                                   const VkImageMemoryBarrier* pImageMemoryBarriers) const
    {
        vkCmdPipelineBarrier(m_Handle, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers,
                             bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    FORCEINLINE void BindPushConstants(VkPipelineLayout pipelineLayout, const VkShaderStageFlags shaderStageFlags, const uint32_t offset,
                                       const uint32_t size, const void* values = VK_NULL_HANDLE) const
    {
        vkCmdPushConstants(m_Handle, pipelineLayout, shaderStageFlags, offset, size, values);
    }

    void BindDescriptorSets(Shared<VulkanPipeline>& pipeline, const uint32_t firstSet = 0, const uint32_t descriptorSetCount = 0,
                            VkDescriptorSet* descriptorSets = VK_NULL_HANDLE, const uint32_t dynamicOffsetCount = 0,
                            uint32_t* dynamicOffsets = nullptr) const;

    // void BindPipeline(Ref<VulkanPipeline>& pipeline) const;
    // void SetPipelinePolygonMode(Ref<VulkanPipeline>& pipeline,
    //                             const EPolygonMode polygonMode) const;  // Invoke before binding actual pipeline

    FORCEINLINE void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                 const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const
    {
        vkCmdDrawIndexed(m_Handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    FORCEINLINE void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                          const uint32_t firstInstance = 0) const
    {
        vkCmdDraw(m_Handle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    FORCEINLINE void DrawIndexedIndirect(const VkBuffer& buffer, const VkDeviceSize offset, const uint32_t drawCount, const uint32_t stride)
    {
        vkCmdDrawIndexedIndirect(m_Handle, buffer, offset, drawCount, stride);
    }

    FORCEINLINE void BindVertexBuffers(const uint32_t firstBinding = 0, const uint32_t bindingCount = 1, VkBuffer* buffers = VK_NULL_HANDLE,
                                       VkDeviceSize* offsets = VK_NULL_HANDLE) const
    {
        PFR_ASSERT(buffers, "Invalid vertex buffer(s)!");
        vkCmdBindVertexBuffers(m_Handle, firstBinding, bindingCount, buffers, offsets);
    }

    FORCEINLINE void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ)
    {
        vkCmdDispatch(m_Handle, groupCountX, groupCountY, groupCountZ);
    }

    FORCEINLINE
    void BindIndexBuffer(const VkBuffer& buffer, const VkDeviceSize offset = 0, VkIndexType indexType = VK_INDEX_TYPE_UINT32) const
    {
        vkCmdBindIndexBuffer(m_Handle, buffer, offset, indexType);
    }

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
                                                const VkAccelerationStructureBuildRangeInfoKHR* const* buildRangeInfos)
    {
        vkCmdBuildAccelerationStructuresKHR(m_Handle, infoCount, infos, buildRangeInfos);
    }

    FORCEINLINE void CopyBuffer(const VkBuffer& srcBuffer, VkBuffer& dstBuffer, const uint32_t regionCount, const VkBufferCopy* pRegions)
    {
        vkCmdCopyBuffer(m_Handle, srcBuffer, dstBuffer, regionCount, pRegions);
    }

    FORCEINLINE void CopyBufferToImage(const VkBuffer& srcBuffer, VkImage& dstImage, const VkImageLayout dstImageLayout,
                                       const uint32_t regionCount, const VkBufferImageCopy* pRegions)
    {
        vkCmdCopyBufferToImage(m_Handle, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }

    FORCEINLINE void BlitImage(const VkImage& srcImage, const VkImageLayout srcImageLayout, VkImage& dstImage,
                               const VkImageLayout dstImageLayout, const uint32_t regionCount, const VkImageBlit* pRegions,
                               const VkFilter filter)
    {
        vkCmdBlitImage(m_Handle, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }

    FORCEINLINE void DrawMeshTasksNV(const uint32_t taskCount, const uint32_t firstTask = 0)
    {
        vkCmdDrawMeshTasksNV(m_Handle, taskCount, firstTask);
    }

  private:
    VkCommandBuffer m_Handle    = VK_NULL_HANDLE;
    ECommandBufferLevel m_Level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY;
    ECommandBufferType m_Type   = ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS;
    VkFence m_SubmitFence       = VK_NULL_HANDLE;

    void Destroy() final override;
    void CreateSyncResourcesAndQueries();
};

}  // namespace Pathfinder

#endif  // VULKANCOMMANDBUFFER_H
