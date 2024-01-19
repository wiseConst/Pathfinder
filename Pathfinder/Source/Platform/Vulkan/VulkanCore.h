#ifndef VULKANCORE_H
#define VULKANCORE_H

#include <volk/volk.h>
#include <Renderer/RendererCoreDefines.h>
#include "Renderer/Shader.h"

namespace Pathfinder
{

#define PFR_VK_API_VERSION VK_API_VERSION_1_3

#define VK_EXCLUSIVE_FULL_SCREEN_TEST 0

#define VK_FORCE_VALIDATION 1
#define VK_FORCE_SHADER_COMPILATION 0
#define VK_FORCE_PIPELINE_COMPILATION 0

#define VK_LOG_INFO 0
#define VK_SHADER_DEBUG_PRINTF 0

#define VK_PREFER_IGPU 0

#define VK_RTX 0
#define VK_MESH_SHADING 1

#if PFR_DEBUG
constexpr static bool s_bEnableValidationLayers = true;
#else
constexpr static bool s_bEnableValidationLayers = false;
#endif

static const std::vector<const char*> s_InstanceLayers     = {"VK_LAYER_KHRONOS_validation"};
static const std::vector<const char*> s_InstanceExtensions = {
#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,        // Required by full screen ext
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,  // Required by full screen ext
};

static const std::vector<const char*> s_DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,          // For rendering into OS-window
    VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,  // To get advanced info about gpu
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,  // To neglect renderpasses as my target is desktop only

#if VK_EXCLUSIVE_FULL_SCREEN_TEST
    VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,  // Borderless fullscreen window
#endif

#if VK_MESH_SHADING
    VK_EXT_MESH_SHADER_EXTENSION_NAME,  // Mesh shading advanced modern rendering,
#endif

    VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,  // For useful pipeline features that can be changed real-time.

#if VK_RTX
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,    // To build acceleration structures
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,      // To use vkCmdTraceRaysKHR
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,  // Required by acceleration structures
#endif
#if VK_SHADER_DEBUG_PRINTF
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,  // debugPrintEXT shaders
#endif
};

class VulkanCommandBuffer;
using VulkanCommandBufferPerFrame = std::array<Shared<VulkanCommandBuffer>, s_FRAMES_IN_FLIGHT>;

using VulkanSemaphorePerFrame      = std::array<VkSemaphore, s_FRAMES_IN_FLIGHT>;
using VulkanDescriptorPoolPerFrame = std::array<VkDescriptorPool, s_FRAMES_IN_FLIGHT>;
using VulkanDescriptorSetPerFrame  = std::array<VkDescriptorSet, s_FRAMES_IN_FLIGHT>;

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

// TODO: Make it #define, to reduce function calls in release mode.
static void VK_SetDebugName(const VkDevice& logicalDevice, const uint64_t objectHandle, const VkObjectType objectType,
                            const char* objectName)
{
    if constexpr (!VK_FORCE_VALIDATION && !s_bEnableValidationLayers) return;

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

NODISCARD static VkImageMemoryBarrier GetImageMemoryBarrier(const VkImage& image, const VkImageAspectFlags aspectMask,
                                                            const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                                            const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                                                            const uint32_t layerCount, const uint32_t baseArrayLayer,
                                                            const uint32_t levelCount, const uint32_t baseMipLevel,
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

NODISCARD static VkCompareOp PathfinderCompareOpToVulkan(const ECompareOp compareOp)
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

NODISCARD static VkShaderStageFlagBits PathfinderShaderStageToVulkan(const EShaderStage shaderStage)
{
    switch (shaderStage)
    {
        case EShaderStage::SHADER_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case EShaderStage::SHADER_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case EShaderStage::SHADER_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case EShaderStage::SHADER_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
        case EShaderStage::SHADER_STAGE_ALL_GRAPHICS: return VK_SHADER_STAGE_ALL_GRAPHICS;
        case EShaderStage::SHADER_STAGE_ALL: return VK_SHADER_STAGE_ALL;
        case EShaderStage::SHADER_STAGE_RAYGEN: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case EShaderStage::SHADER_STAGE_ANY_HIT: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case EShaderStage::SHADER_STAGE_CLOSEST_HIT: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case EShaderStage::SHADER_STAGE_MISS: return VK_SHADER_STAGE_MISS_BIT_KHR;
        case EShaderStage::SHADER_STAGE_INTERSECTION: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case EShaderStage::SHADER_STAGE_CALLABLE: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case EShaderStage::SHADER_STAGE_TASK: return VK_SHADER_STAGE_TASK_BIT_EXT;
        case EShaderStage::SHADER_STAGE_MESH: return VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    PFR_ASSERT(false, "Unknown shader stage!");
    return VK_SHADER_STAGE_VERTEX_BIT;
}

NODISCARD static uint32_t GetTypeSizeFromVulkanFormat(const VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_UNDEFINED: return 0;

        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT: return sizeof(int16_t);

        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT: return sizeof(int32_t);

        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64_SFLOAT: return sizeof(int64_t);

        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT: return 2 * sizeof(int16_t);

        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT: return 2 * sizeof(int32_t);

        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64_SFLOAT: return 2 * sizeof(int64_t);

        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT: return 3 * sizeof(int16_t);

        case VK_FORMAT_R32G32B32_SFLOAT:
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT: return 3 * sizeof(int32_t);

        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64_SFLOAT: return 3 * sizeof(int64_t);

        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT: return 4 * sizeof(int16_t);

        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 4 * sizeof(int32_t);

        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_R64G64B64A64_SFLOAT: return 4 * sizeof(int64_t);
    }

    PFR_ASSERT(false, "Unknown type format!");
    return 0;
}

NODISCARD static VkDescriptorSetAllocateInfo GetDescriptorSetAllocateInfo(const VkDescriptorPool& descriptorPool,
                                                                          const uint32_t descriptorSetCount,
                                                                          const VkDescriptorSetLayout* descriptorSetLayouts)
{
    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool,
                                                                   descriptorSetCount, descriptorSetLayouts};
    return descriptorSetAllocateInfo;
}

NODISCARD static VkDescriptorPoolCreateInfo GetDescriptorPoolCreateInfo(const uint32_t poolSizeCount, const uint32_t maxSetCount,
                                                                        const VkDescriptorPoolSize* poolSizes,
                                                                        const VkDescriptorPoolCreateFlags descriptorPoolCreateFlags = 0,
                                                                        const void* pNext = nullptr)
{
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, pNext, descriptorPoolCreateFlags, maxSetCount, poolSizeCount, poolSizes};
    return descriptorPoolCreateInfo;
}

NODISCARD static VkPushConstantRange GetPushConstantRange(const VkShaderStageFlags stageFlags, const uint32_t offset, const uint32_t size)
{
    const VkPushConstantRange pushConstantRange = {stageFlags, offset, size};
    return pushConstantRange;
}

}  // namespace VulkanUtility

}  // namespace Pathfinder

#endif  // VULKANCORE_H
