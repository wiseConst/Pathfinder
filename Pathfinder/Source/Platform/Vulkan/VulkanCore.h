#ifndef VULKANCORE_H
#define VULKANCORE_H

#include <volk/volk.h>
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

#define PFR_VK_API_VERSION VK_API_VERSION_1_3

#define VK_PREFER_IGPU 0
#define VK_FORCE_VALIDATION 1
#define VK_LOG_INFO 0

#if PFR_DEBUG
constexpr static bool s_bEnableValidationLayers = true;
#else
constexpr static bool s_bEnableValidationLayers = false;
#endif

static const std::vector<const char*> s_InstanceLayers     = {"VK_LAYER_KHRONOS_validation"};
static const std::vector<const char*> s_InstanceExtensions = {
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,        // Required by full screen ext
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,  // Required by full screen ext
};

static const std::vector<const char*> s_DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,          // For rendering into OS-window
    VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,  // To get advanced info about gpu
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,  // To neglect renderpasses as my target is desktop only

    VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,  // Borderless fullscreen window

    VK_EXT_MESH_SHADER_EXTENSION_NAME,  // Mesh shading advanced modern rendering,

    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,  // For useful pipeline features that can be changed real-time.

    // VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,    // To build acceleration structures
    // VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,      // To use vkCmdTraceRaysKHR
    // VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,  // Required by acceleration structures
};

class VulkanCommandBuffer;
using VulkanCommandBufferPerFrame = std::array<Shared<VulkanCommandBuffer>, s_FRAMES_IN_FLIGHT>;

using VulkanSempahorePerFrame = std::array<VkSemaphore, s_FRAMES_IN_FLIGHT>;

static std::string VK_GetResultString(const VkResult result)
{
    const char* resultString = "Unknown";

#define STR(a)                                                                                                                             \
    case a: resultString = #a; break;

    switch (result)
    {
        STR(VK_SUCCESS);
        STR(VK_NOT_READY);
        STR(VK_TIMEOUT);
        STR(VK_EVENT_SET);
        STR(VK_EVENT_RESET);
        STR(VK_INCOMPLETE);
        STR(VK_ERROR_OUT_OF_HOST_MEMORY);
        STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        STR(VK_ERROR_INITIALIZATION_FAILED);
        STR(VK_ERROR_DEVICE_LOST);
        STR(VK_ERROR_MEMORY_MAP_FAILED);
        STR(VK_ERROR_LAYER_NOT_PRESENT);
        STR(VK_ERROR_EXTENSION_NOT_PRESENT);
        STR(VK_ERROR_FEATURE_NOT_PRESENT);
        STR(VK_ERROR_INCOMPATIBLE_DRIVER);
        STR(VK_ERROR_TOO_MANY_OBJECTS);
        STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
        STR(VK_ERROR_FRAGMENTED_POOL);
        STR(VK_ERROR_OUT_OF_POOL_MEMORY);
        STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        STR(VK_ERROR_SURFACE_LOST_KHR);
        STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(VK_SUBOPTIMAL_KHR);
        STR(VK_ERROR_OUT_OF_DATE_KHR);
        STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(VK_ERROR_VALIDATION_FAILED_EXT);
        STR(VK_ERROR_INVALID_SHADER_NV);
        STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        STR(VK_ERROR_FRAGMENTATION_EXT);
        STR(VK_ERROR_NOT_PERMITTED_EXT);
        STR(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT);
        STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    }
#undef STR
    return resultString;
}

static void VK_CHECK(const VkResult result, const char* message)
{
    const std::string finalMessage = std::string(message) + std::string(" The result is: ") + std::string(VK_GetResultString(result));
    PFR_ASSERT(result == VK_SUCCESS, finalMessage.data());
}

static void VK_SetDebugName(const VkDevice& logicalDevice, const uint64_t objectHandle, const VkObjectType objectType,
                            const char* objectName)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    objectNameInfo.objectHandle                  = objectHandle;
    objectNameInfo.objectType                    = objectType;
    objectNameInfo.pObjectName                   = objectName;
    VK_CHECK(vkSetDebugUtilsObjectNameEXT(logicalDevice, &objectNameInfo), "Failed to set debug name!");
}

static VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        {
            if constexpr (VK_LOG_INFO) LOG_INFO(pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            if constexpr (VK_LOG_INFO) LOG_INFO(pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            LOG_WARN(pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            LOG_ERROR(pCallbackData->pMessage);
            break;
        }
    }

    return VK_FALSE;
}

namespace VulkanUtility
{

static VkImageMemoryBarrier GetImageMemoryBarrier(const VkImage& image, const VkImageAspectFlags aspectMask, const VkImageLayout oldLayout,
                                                  const VkImageLayout newLayout, const VkAccessFlags srcAccessMask,
                                                  const VkAccessFlags dstAccessMask, const uint32_t layerCount,
                                                  const uint32_t baseArrayLayer, const uint32_t levelCount, const uint32_t baseMipLevel,
                                                  const uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                  const uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
{
    VkImageMemoryBarrier imageMemoryBarrier            = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.image                           = image;
    imageMemoryBarrier.oldLayout                       = oldLayout;
    imageMemoryBarrier.newLayout                       = newLayout;
    imageMemoryBarrier.srcAccessMask                   = srcAccessMask;
    imageMemoryBarrier.dstAccessMask                   = dstAccessMask;
    imageMemoryBarrier.subresourceRange.aspectMask     = aspectMask;
    imageMemoryBarrier.subresourceRange.layerCount     = layerCount;
    imageMemoryBarrier.subresourceRange.levelCount     = levelCount;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    imageMemoryBarrier.subresourceRange.baseMipLevel   = baseMipLevel;

    imageMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    imageMemoryBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;

    return imageMemoryBarrier;
}

}  // namespace VulkanUtility

}  // namespace Pathfinder

#endif  // VULKANCORE_H
