#pragma once

#include <volk/volk.h>
#include "Globals.h"
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

#ifdef SHADER_DEBUG_PRINTF
#undef SHADER_DEBUG_PRINTF
#define SHADER_DEBUG_PRINTF 0
#endif

#define PFR_VK_API_VERSION VK_API_VERSION_1_3

#define VK_FORCE_VALIDATION 1
#define VK_FORCE_SHADER_COMPILATION 1
#define VK_FORCE_PIPELINE_COMPILATION 0
#define VK_FORCE_DRIVER_PIPELINE_CACHE 1

#define VK_LOG_VMA_ALLOCATIONS 0
#define VK_LOG_INFO 0

#define RENDERDOC_DEBUG 0

#if PFR_DEBUG
constexpr static bool s_bEnableValidationLayers = true;
#else
constexpr static bool s_bEnableValidationLayers = false;
#endif

static const std::vector<const char*> s_InstanceLayers = {
    "VK_LAYER_KHRONOS_validation",  // Validation layers
#ifdef PFR_LINUX
    "VK_LAYER_MESA_overlay",  // Linux statistics overlay(framerate graph etc)
#endif
};

static const std::vector<const char*> s_InstanceExtensions = {
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,  // Required by full screen ext
};

static const std::vector<const char*> s_DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,                     // For rendering into OS-window
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,             // To neglect render-passes as my target is desktop only
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,            // To do async work with proper synchronization on device side.
    VK_KHR_MAINTENANCE_4_EXTENSION_NAME,                 // Shader SPIRV 1.6
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,             // Advanced synchronization types
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,                 // Provides query for current memory usage and budget.
    VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,  // It will allow device-local memory allocations to be paged in and out by the
                                                         // operating system, and may not return VK_ERROR_OUT_OF_DEVICE_MEMORY even if
                                                         // device-local memory appears to be full, but will instead page this, or other
                                                         // allocations, out to make room. The Vulkan impl will also ensure that
                                                         // host-local memory allocations will never be promoted to device-local memory by
                                                         // the operating system, or consume device-local memory.
    VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,               // Required by PAGEABLE_DEVICE_LOCAL_MEMORY

    VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,  // Exclusive fullscreen window

    VK_EXT_MESH_SHADER_EXTENSION_NAME,  // Mesh shading

    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,  // For useful pipeline features that can be changed real-time.

#if !RENDERDOC_DEBUG
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,  // To build acceleration structures

    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,  // To use vkCmdTraceRaysKHR

    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,  // Required by acceleration structure,
    // allows the driver to run some expensive CPU-based Vulkan API calls asynchronously(such as a Vulkan API call that builds an
    // acceleration structure on a CPU instead of a GPU) — much like launching a thread in C++ to perform a task asynchronously, then
    // waiting for it to complete.

    VK_KHR_RAY_QUERY_EXTENSION_NAME,  // To trace rays in every shader I want
#endif

#if SHADER_DEBUG_PRINTF
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,  // debugPrintEXT shaders
#endif
};

NODISCARD static std::string VK_GetResultString(const VkResult result)
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

#if PFR_RELEASE
#define VK_CHECK(Vk_Result, message)                                                                                                       \
    {                                                                                                                                      \
        const auto result              = (Vk_Result);                                                                                      \
        const std::string finalMessage = std::string(message) + std::string(" The result is: ") + std::string(VK_GetResultString(result)); \
        if (result != VK_SUCCESS) LOG_ERROR("{}", finalMessage);                                                                           \
        PFR_ASSERT(result == VK_SUCCESS, finalMessage);                                                                                    \
    }
#elif PFR_DEBUG
#define VK_CHECK(Vk_Result, message)                                                                                                       \
    {                                                                                                                                      \
        const auto result              = (Vk_Result);                                                                                      \
        const std::string finalMessage = std::string(message) + std::string(" The result is: ") + std::string(VK_GetResultString(result)); \
        PFR_ASSERT(result == VK_SUCCESS, finalMessage);                                                                                    \
    }
#else
#error Unknown configuration type!
#endif

#if (VK_FORCE_VALIDATION || s_bEnableValidationLayers)
#define VK_SetDebugName(logicalDevice, handle, type, name)                                                                                 \
    {                                                                                                                                      \
        PFR_ASSERT(handle, "Object handle is not valid!");                                                                                 \
        const VkDebugUtilsObjectNameInfoEXT duoni = {.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,                   \
                                                     .objectType   = type,                                                                 \
                                                     .objectHandle = (uint64_t)handle,                                                     \
                                                     .pObjectName  = name};                                                                 \
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(logicalDevice, &duoni), "Failed to set debug name!");                                        \
    }
#else
#define VK_SetDebugName(logicalDevice, objectHandle, objectType, objectName)
#endif

static VkBool32 VK_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, [[maybe_unused]] void* pUserData)
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
            if constexpr (VK_LOG_INFO || SHADER_DEBUG_PRINTF) LOG_INFO(pCallbackData->pMessage);
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

namespace VulkanUtils
{

NODISCARD FORCEINLINE static VkImageMemoryBarrier2 GetImageMemoryBarrier(
    const VkImage& image, const VkImageAspectFlags aspectMask, const VkImageLayout oldLayout, const VkImageLayout newLayout,
    const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 srcAccessMask, const VkPipelineStageFlags2 dstStageMask,
    const VkAccessFlags2 dstAccessMask, const uint32_t layerCount, const uint32_t baseArrayLayer, const uint32_t levelCount,
    const uint32_t baseMipLevel, const uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    const uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, void* pNext = nullptr)
{
    return VkImageMemoryBarrier2{.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .pNext               = pNext,
                                 .srcStageMask        = srcStageMask,
                                 .srcAccessMask       = srcAccessMask,
                                 .dstStageMask        = dstStageMask,
                                 .dstAccessMask       = dstAccessMask,
                                 .oldLayout           = oldLayout,
                                 .newLayout           = newLayout,
                                 .srcQueueFamilyIndex = srcQueueFamilyIndex,
                                 .dstQueueFamilyIndex = dstQueueFamilyIndex,
                                 .image               = image,
                                 .subresourceRange    = {.aspectMask     = aspectMask,
                                                         .baseMipLevel   = baseMipLevel,
                                                         .levelCount     = levelCount,
                                                         .baseArrayLayer = baseArrayLayer,
                                                         .layerCount     = layerCount}};
}

NODISCARD FORCEINLINE static VkBufferMemoryBarrier2 GetBufferMemoryBarrier(
    const VkBuffer& buffer, const VkDeviceSize size, const VkDeviceSize offset, const VkPipelineStageFlags2 srcStageMask,
    const VkAccessFlags2 srcAccessMask, const VkPipelineStageFlags2 dstStageMask, const VkAccessFlags2 dstAccessMask,
    const uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, const uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    void* pNext = nullptr)
{
    return VkBufferMemoryBarrier2{.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                  .pNext               = pNext,
                                  .srcStageMask        = srcStageMask,
                                  .srcAccessMask       = srcAccessMask,
                                  .dstStageMask        = dstStageMask,
                                  .dstAccessMask       = dstAccessMask,
                                  .srcQueueFamilyIndex = srcQueueFamilyIndex,
                                  .dstQueueFamilyIndex = dstQueueFamilyIndex,
                                  .buffer              = buffer,
                                  .offset              = offset,
                                  .size                = size};
}

NODISCARD FORCEINLINE static VkDescriptorSetAllocateInfo GetDescriptorSetAllocateInfo(const VkDescriptorPool& descriptorPool,
                                                                                      const uint32_t descriptorSetCount,
                                                                                      const VkDescriptorSetLayout* descriptorSetLayouts,
                                                                                      void* pNext = nullptr)
{
    return VkDescriptorSetAllocateInfo{.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                       .pNext              = pNext,
                                       .descriptorPool     = descriptorPool,
                                       .descriptorSetCount = descriptorSetCount,
                                       .pSetLayouts        = descriptorSetLayouts};
}

NODISCARD FORCEINLINE static VkDescriptorPoolCreateInfo GetDescriptorPoolCreateInfo(
    const uint32_t poolSizeCount, const uint32_t maxSetCount, const VkDescriptorPoolSize* poolSizes,
    const VkDescriptorPoolCreateFlags descriptorPoolCreateFlags = 0, const void* pNext = nullptr)
{
    return VkDescriptorPoolCreateInfo{.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .pNext         = pNext,
                                      .flags         = descriptorPoolCreateFlags,
                                      .maxSets       = maxSetCount,
                                      .poolSizeCount = poolSizeCount,
                                      .pPoolSizes    = poolSizes};
}

NODISCARD FORCEINLINE static VkPushConstantRange GetPushConstantRange(const VkShaderStageFlags stageFlags, const uint32_t offset,
                                                                      const uint32_t size)
{
    return VkPushConstantRange{.stageFlags = stageFlags, .offset = offset, .size = size};
}

NODISCARD FORCEINLINE static VkPolygonMode PathfinderPolygonModeToVulkan(const EPolygonMode polygonMode)
{
    switch (polygonMode)
    {
        case EPolygonMode::POLYGON_MODE_FILL: return VK_POLYGON_MODE_FILL;
        case EPolygonMode::POLYGON_MODE_LINE: return VK_POLYGON_MODE_LINE;
        case EPolygonMode::POLYGON_MODE_POINT: return VK_POLYGON_MODE_POINT;
    }

    PFR_ASSERT(false, "Unknown polygon mode!");
    return VK_POLYGON_MODE_FILL;
}

NODISCARD FORCEINLINE static VkCompareOp PathfinderCompareOpToVulkan(const ECompareOp compareOp)
{
    switch (compareOp)
    {
        case ECompareOp::COMPARE_OP_LESS: return VK_COMPARE_OP_LESS;
        case ECompareOp::COMPARE_OP_EQUAL: return VK_COMPARE_OP_EQUAL;
        case ECompareOp::COMPARE_OP_NEVER: return VK_COMPARE_OP_NEVER;
        case ECompareOp::COMPARE_OP_ALWAYS: return VK_COMPARE_OP_ALWAYS;
        case ECompareOp::COMPARE_OP_GREATER: return VK_COMPARE_OP_GREATER;
        case ECompareOp::COMPARE_OP_NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
        case ECompareOp::COMPARE_OP_LESS_OR_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case ECompareOp::COMPARE_OP_GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    }

    PFR_ASSERT(false, "Unknown compare op!");
    return VK_COMPARE_OP_NEVER;
}

}  // namespace VulkanUtils

}  // namespace Pathfinder
