#ifndef VULKANDEVICE_H
#define VULKANDEVICE_H

#include "VulkanCore.h"
#include "Renderer/CommandBuffer.h"

namespace Pathfinder
{

struct QueueFamilyIndices
{
    FORCEINLINE bool IsComplete() const
    {
        return GraphicsFamily != UINT32_MAX && PresentFamily != UINT32_MAX && ComputeFamily != UINT32_MAX && TransferFamily != UINT32_MAX;
    }

    static QueueFamilyIndices FindQueueFamilyIndices(const VkPhysicalDevice& physicalDevice)
    {
        QueueFamilyIndices indices = {};

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        PFR_ASSERT(queueFamilyCount != 0, "Looks like your gpu doesn't have any queues to submit commands to.");

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        // Firstly choose queue indices then log info if required.
        uint32_t i = 0;
        for (const auto& family : queueFamilies)
        {
            if (family.queueCount <= 0)
            {
                ++i;
                continue;
            }

            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT && indices.GraphicsFamily == UINT32_MAX)
            {
                indices.GraphicsFamily = i;
            }
            else if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && indices.ComputeFamily == UINT32_MAX)  // dedicated compute queue
            {
                indices.ComputeFamily = i;
            }
            else if (family.queueFlags & VK_QUEUE_TRANSFER_BIT && indices.TransferFamily == UINT32_MAX)  // dedicated transfer queue
            {
                indices.TransferFamily = i;
            }

            if (indices.GraphicsFamily == i)
            {
                VkBool32 bPresentSupport{VK_FALSE};

#if PFR_WINDOWS
                bPresentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i);
#elif PFR_LINUX
                // TODO:
                bPresentSupport = vkGetPhysicalDeviceWaylandPresentationSupportKHR();
#elif PFR_MACOS
                // TODO:
                // On macOS, all physical devices and queue families must be capable of presentation with any layer.
                // As a result there is no macOS-specific query for these capabilities.
                bPresentSupport = true;
#endif

                if (bPresentSupport)
                    indices.PresentFamily = indices.GraphicsFamily;
                else
                    PFR_ASSERT(false, "Present family should be the same as graphics family!");
            }

            if (indices.IsComplete()) break;
            ++i;
        }

        // From vulkan-tutorial.com: Any queue family with VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT capabilities already implicitly
        // support VK_QUEUE_TRANSFER_BIT operations.
        if (indices.TransferFamily == UINT32_MAX) indices.TransferFamily = indices.GraphicsFamily;

        // Rely on that graphics queue will be from first family, which contains everything.
        if (indices.ComputeFamily == UINT32_MAX) indices.ComputeFamily = indices.GraphicsFamily;

#if VK_LOG_INFO
        LOG_INFO("QueueFamilyCount:%u", queueFamilyCount);

        i = 0;
        for (const auto& family : queueFamilies)
        {
            LOG_TRACE(" [%d] queueFamily has (%u) queue(s) which supports operations:", i + 1, family.queueCount);

            if (family.queueCount <= 0)
            {
                ++i;
                continue;
            }

            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                LOG_TRACE("  GRAPHICS");
            }

            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                LOG_TRACE("  TRANSFER");
            }

            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                LOG_TRACE("  COMPUTE");
            }

            VkBool32 bPresentSupport{VK_FALSE};
#if PFR_WINDOWS
            bPresentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i);
#elif PFR_LINUX
            // TODO:
            bPresentSupport = vkGetPhysicalDeviceWaylandPresentationSupportKHR();
#elif PFR_MACOS
            // TODO:
            // On macOS, all physical devices and queue families must be capable of presentation with any layer.
            // As a result there is no macOS-specific query for these capabilities.
            bPresentSupport = true;
#endif

            if (bPresentSupport)
            {
                LOG_TRACE("  PRESENT");
            }

            if (family.queueFlags & VK_QUEUE_PROTECTED_BIT)
            {
                LOG_TRACE("  PROTECTED");
            }

            if (family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            {
                LOG_TRACE("  SPARSE BINDING");
            }

            if (family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
            {
                LOG_TRACE("  VIDEO DECODE");
            }

#ifdef VK_ENABLE_BETA_EXTENSIONS
            if (family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
            {
                LOG_TRACE("  VIDEO ENCODE");
            }
#endif

            if (family.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
            {
                LOG_TRACE("  OPTICAL FLOW NV");
            }

            ++i;
        }
#endif

        PFR_ASSERT(indices.IsComplete(), "QueueFamilyIndices wasn't setup correctly!");
        return indices;
    }

    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t PresentFamily  = UINT32_MAX;
    uint32_t ComputeFamily  = UINT32_MAX;
    uint32_t TransferFamily = UINT32_MAX;
};

class VulkanAllocator;
class VulkanDescriptorAllocator;
class VulkanDevice final : private Uncopyable, private Unmovable
{
  public:
    explicit VulkanDevice(const VkInstance& instance);
    VulkanDevice() = delete;
    ~VulkanDevice() override;

    void AllocateCommandBuffer(VkCommandBuffer& inOutCommandBuffer, ECommandBufferType type,
                               VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, const uint16_t threadID = 0) const;
    void FreeCommandBuffer(const VkCommandBuffer& commandBuffer, ECommandBufferType type, const uint16_t threadID = 0) const;

    NODISCARD FORCEINLINE const auto& GetAllocator() const { return m_VMA; }
    NODISCARD FORCEINLINE const auto& GetDescriptorAllocator() const { return m_VDA; }

    FORCEINLINE void WaitDeviceOnFinish() const
    {
        PFR_ASSERT(vkDeviceWaitIdle(m_GPUInfo.LogicalDevice) == VK_SUCCESS,
                   "Failed to wait for rendering device to finish other operations.");
    }

    NODISCARD FORCEINLINE const auto& GetLogicalDevice() const { return m_GPUInfo.LogicalDevice; }
    NODISCARD FORCEINLINE const auto& GetPhysicalDevice() const { return m_GPUInfo.PhysicalDevice; }
    NODISCARD FORCEINLINE const auto& GetGPUProperties() const { return m_GPUInfo.GPUProperties; }

    NODISCARD FORCEINLINE const auto& GetQueueFamilyIndices() const { return m_GPUInfo.QueueFamilyIndices; }
    NODISCARD FORCEINLINE const auto& GetGraphicsQueue() const { return m_GPUInfo.GraphicsQueue; }
    NODISCARD FORCEINLINE const auto& GetPresentQueue() const { return m_GPUInfo.PresentQueue; }
    NODISCARD FORCEINLINE const auto& GetComputeQueue() const { return m_GPUInfo.ComputeQueue; }
    NODISCARD FORCEINLINE const auto& GetTransferQueue() const { return m_GPUInfo.TransferQueue; }

    NODISCARD FORCEINLINE VkDeviceAddress GetBufferDeviceAddress(const VkBuffer& buffer) const
    {
        const VkBufferDeviceAddressInfo bdaInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
        return vkGetBufferDeviceAddress(m_GPUInfo.LogicalDevice, &bdaInfo);
    }

  private:
    struct GPUInfo
    {
        VkDevice LogicalDevice          = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        std::vector<VkFormat> SupportedDepthStencilFormats;

        Pathfinder::QueueFamilyIndices QueueFamilyIndices = {};
        std::array<VkCommandPool, s_WORKER_THREAD_COUNT> GraphicsCommandPools;
        VkQueue GraphicsQueue = VK_NULL_HANDLE;
        VkQueue PresentQueue  = VK_NULL_HANDLE;

        std::array<VkCommandPool, s_WORKER_THREAD_COUNT> TransferCommandPools;
        VkQueue TransferQueue = VK_NULL_HANDLE;

        std::array<VkCommandPool, s_WORKER_THREAD_COUNT> ComputeCommandPools;
        VkQueue ComputeQueue = VK_NULL_HANDLE;

        VkPhysicalDeviceProperties GPUProperties             = {};
        VkPhysicalDeviceFeatures GPUFeatures                 = {};
        VkPhysicalDeviceMemoryProperties GPUMemoryProperties = {};
        VkPhysicalDeviceDriverProperties GPUDriverProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};

        VkPhysicalDeviceMeshShaderPropertiesEXT MSProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
        bool bMeshShaderSupport                              = true;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR RTProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceAccelerationStructurePropertiesKHR ASProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        bool bRTXSupport = false;
    } m_GPUInfo;
    Unique<VulkanAllocator> m_VMA;
    Unique<VulkanDescriptorAllocator> m_VDA;

    void PickPhysicalDevice(const VkInstance& instance);
    void CreateLogicalDevice();
    void CreateCommandPools();

    uint32_t RateDeviceSuitability(GPUInfo& gpuInfo);
    bool IsDeviceSuitable(GPUInfo& gpuInfo);
    bool CheckDeviceExtensionSupport(GPUInfo& gpuInfo);
};

}  // namespace Pathfinder

#endif  // VULKANDEVICE_H
