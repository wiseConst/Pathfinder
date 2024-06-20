#pragma once

#include "VulkanCore.h"
#include <Renderer/CommandBuffer.h>
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

class VulkanAllocator;
class VulkanDevice final : private Uncopyable, private Unmovable
{
  public:
    explicit VulkanDevice(const VkInstance& instance);
    ~VulkanDevice();

    void AllocateCommandBuffer(VkCommandBuffer& inOutCommandBuffer, const CommandBufferSpecification& commandBufferSpec) const;
    void FreeCommandBuffer(const VkCommandBuffer& commandBuffer, const CommandBufferSpecification& commandBufferSpec) const;

    NODISCARD FORCEINLINE const auto& GetAllocator() const { return m_VMA; }

    FORCEINLINE void WaitDeviceOnFinish() const
    {
        PFR_ASSERT(vkDeviceWaitIdle(m_LogicalDevice) == VK_SUCCESS, "Failed to wait for rendering device to finish other operations.");
    }

    NODISCARD FORCEINLINE const auto GetMaxSamplerAnisotropy() const { return m_MaxSamplerAnisotropy; }
    NODISCARD FORCEINLINE const auto GetTimestampPeriod() const { return m_TimestampPeriod; }
    NODISCARD FORCEINLINE const auto& GetLogicalDevice() const { return m_LogicalDevice; }
    NODISCARD FORCEINLINE const auto& GetPhysicalDevice() const { return m_PhysicalDevice; }

    NODISCARD FORCEINLINE const auto& GetGraphicsQueue() const { return m_GraphicsQueue; }
    NODISCARD FORCEINLINE const auto& GetPresentQueue() const { return m_PresentQueue; }
    NODISCARD FORCEINLINE const auto& GetComputeQueue() const { return m_ComputeQueue; }
    NODISCARD FORCEINLINE const auto& GetTransferQueue() const { return m_TransferQueue; }

    NODISCARD FORCEINLINE const auto GetGraphicsFamily() const { return m_GraphicsFamily; }
    NODISCARD FORCEINLINE const auto GetPresentFamily() const { return m_PresentFamily; }
    NODISCARD FORCEINLINE const auto GetComputeFamily() const { return m_ComputeFamily; }
    NODISCARD FORCEINLINE const auto GetTransferFamily() const { return m_TransferFamily; }

    NODISCARD FORCEINLINE VkDeviceAddress GetAccelerationStructureAddress(const VkAccelerationStructureKHR& as) const
    {
        const VkAccelerationStructureDeviceAddressInfoKHR asdaInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = as};
        return vkGetAccelerationStructureDeviceAddressKHR(m_LogicalDevice, &asdaInfo);
    }

    NODISCARD FORCEINLINE VkDeviceAddress GetBufferDeviceAddress(const VkBuffer& buffer) const
    {
        const VkBufferDeviceAddressInfo bdaInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer};
        return vkGetBufferDeviceAddress(m_LogicalDevice, &bdaInfo);
    }

    FORCEINLINE void GetASBuildSizes(VkAccelerationStructureBuildTypeKHR buildType,
                                     const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts,
                                     VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) const
    {
        vkGetAccelerationStructureBuildSizesKHR(m_LogicalDevice, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
    }

    // Should be called each frame.
    void ResetCommandPools() const;

    NODISCARD FORCEINLINE const auto& GetPipelineCache() const { return m_PipelineCache; }
    NODISCARD FORCEINLINE const auto& GetPipelineCacheUUID() const { return m_PipelineCacheUUID; }
    NODISCARD FORCEINLINE const auto GetVendorID() const { return m_VendorID; }
    NODISCARD FORCEINLINE const auto GetDeviceID() const { return m_DeviceID; }
    NODISCARD FORCEINLINE const auto& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

    NODISCARD bool IsDepthStencilFormatSupported(const EImageFormat imageFormat) const;

  private:
    std::vector<VkFormat> m_SupportedDepthStencilFormats;

    using VulkanCommandPoolPerFrame = std::array<std::array<VkCommandPool, s_WORKER_THREAD_COUNT>, s_FRAMES_IN_FLIGHT>;
    VulkanCommandPoolPerFrame m_GraphicsCommandPools;
    VulkanCommandPoolPerFrame m_TransferCommandPools;
    VulkanCommandPoolPerFrame m_ComputeCommandPools;

    std::string m_DeviceName          = s_DEFAULT_STRING;
    VkDevice m_LogicalDevice          = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;  // GCT queue
    VkQueue m_PresentQueue  = VK_NULL_HANDLE;
    VkQueue m_TransferQueue = VK_NULL_HANDLE;
    VkQueue m_ComputeQueue  = VK_NULL_HANDLE;

    std::vector<uint32_t> m_QueueFamilyIndices;
    uint32_t m_GraphicsFamily    = UINT32_MAX;
    uint32_t m_PresentFamily     = UINT32_MAX;
    uint32_t m_ComputeFamily     = UINT32_MAX;
    uint32_t m_TransferFamily    = UINT32_MAX;
    float m_TimestampPeriod      = 1.f;
    float m_MaxSamplerAnisotropy = 0.f;

    uint32_t m_VendorID                       = 0;
    uint32_t m_DeviceID                       = 0;
    uint8_t m_PipelineCacheUUID[VK_UUID_SIZE] = {0};
    VkPipelineCache m_PipelineCache           = VK_NULL_HANDLE;

    Unique<VulkanAllocator> m_VMA;

    VulkanDevice() = delete;
    void ChooseBestPhysicalDevice(const VkInstance& instance);
    void CreateLogicalDevice();
    void CreateCommandPools();
    void SavePipelineCache();
    void LoadPipelineCache();
};

}  // namespace Pathfinder
