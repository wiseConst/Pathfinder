#pragma once

#include "VulkanCore.h"

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
                PFR_ASSERT(family.timestampValidBits != 0, "Queue family doesn't support timestamp queries!");
            }
            else if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && indices.ComputeFamily == UINT32_MAX)  // Dedicated async-compute queue
            {
                indices.ComputeFamily = i;
                PFR_ASSERT(family.timestampValidBits != 0, "Queue family doesn't support timestamp queries!");
            }

            const bool bDMAQueue = (family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&           //
                                   !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&          //
                                   !(family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&           //
                                   !(family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) &&  //
#ifdef VK_ENABLE_BETA_EXTENSIONS
                                   !(family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) &&  //
#endif
                                   !(family.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV);
            if (bDMAQueue && indices.TransferFamily == UINT32_MAX)  // Dedicated transfer queue for DMA
            {
                LOG_TRACE("Found DMA queue!");
                indices.TransferFamily = i;
            }

            if (indices.GraphicsFamily == i)
            {
                VkBool32 bPresentSupport{VK_FALSE};

#if PFR_WINDOWS
                bPresentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i);
#elif PFR_LINUX
                bPresentSupport = vkGetPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, i, glfwGetWaylandDisplay());
#elif PFR_MACOS
                // NOTE:
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

        PFR_ASSERT(indices.IsComplete(), "QueueFamilyIndices wasn't setup correctly!");
        return indices;
    }

    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t PresentFamily  = UINT32_MAX;
    uint32_t ComputeFamily  = UINT32_MAX;
    uint32_t TransferFamily = UINT32_MAX;
};

class VulkanDevice;

// TODO: Add support for multiple devices, base DeviceManager class as well as Device.

class VulkanDeviceManager final
{
  public:
    static Unique<VulkanDevice> ChooseBestFitDevice(const VkInstance& instance);

  private:
    struct GPUInfo
    {
        VkPhysicalDeviceProperties Properties             = {};
        VkPhysicalDeviceDriverProperties DriverProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceMemoryProperties MemoryProperties = {};
        VkPhysicalDeviceFeatures Features                 = {};

        VkPhysicalDeviceMeshShaderPropertiesEXT MSProperties            = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
        VkPhysicalDeviceAccelerationStructurePropertiesKHR ASProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR RTProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

        Pathfinder::QueueFamilyIndices QueueFamilyIndices = {};

        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    };

    static uint32_t RateDeviceSuitability(GPUInfo& gpuInfo);
    static bool IsDeviceSuitable(GPUInfo& gpuInfo);
    static bool CheckDeviceExtensionSupport(GPUInfo& gpuInfo);
    static const char* GetVendorNameCString(const uint32_t vendorID);
    static const char* GetDeviceTypeCString(const VkPhysicalDeviceType deviceType);
};

}  // namespace Pathfinder

