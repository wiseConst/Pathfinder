#pragma once

#include <volk/volk.h>
#include <Renderer/RendererCoreDefines.h>
#include <Renderer/Buffer.h>
#include <Renderer/Shader.h>
#include "Globals.h"

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
    "VK_LAYER_KHRONOS_validation",  // Validaiton layers
#ifdef PFR_LINUX
    "VK_LAYER_MESA_overlay",  // Linux statistics overlay(framerate graph etc)
#endif

};
static const std::vector<const char*> s_InstanceExtensions = {
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,  // Required by full screen ext
};

static const std::vector<const char*> s_DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,                     // For rendering into OS-window
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,             // To neglect renderpasses as my target is desktop only
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,            // To do async work with proper synchronization on device side.
    VK_KHR_MAINTENANCE_4_EXTENSION_NAME,                 // Shader SPIRV 1.6
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,             // Advanced synchronization types
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,                 // Provides query for current memory usage and budget.
    VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME,  // It will allow device-local memory allocations to be paged in and out by the
                                                         // operating system, and may not return VK_ERROR_OUT_OF_DEVICE_MEMORY even if
                                                         // device-local memory appears to be full, but will instead page this, or other
                                                         // allocations, out to make room. The Vulkan implementation will also ensure that
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

class VulkanCommandBuffer;
using VulkanCommandBufferPerFrame = std::array<Shared<VulkanCommandBuffer>, s_FRAMES_IN_FLIGHT>;

using VulkanSemaphorePerFrame      = std::array<VkSemaphore, s_FRAMES_IN_FLIGHT>;
using VulkanFencePerFrame          = std::array<VkFence, s_FRAMES_IN_FLIGHT>;
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
        const VkDebugUtilsObjectNameInfoEXT objectNI = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, type,                 \
                                                        (uint64_t)handle, name};                                                           \
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(logicalDevice, &objectNI), "Failed to set debug name!");                                     \
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

namespace VulkanUtility
{

NODISCARD FORCEINLINE static VkImageMemoryBarrier2 GetImageMemoryBarrier(
    const VkImage& image, const VkImageAspectFlags aspectMask, const VkImageLayout oldLayout, const VkImageLayout newLayout,
    const VkPipelineStageFlags2 srcStageMask, const VkAccessFlags2 srcAccessMask, const VkPipelineStageFlags2 dstStageMask,
    const VkAccessFlags2 dstAccessMask, const uint32_t layerCount, const uint32_t baseArrayLayer, const uint32_t levelCount,
    const uint32_t baseMipLevel, const uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    const uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
{
    return VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 nullptr,
                                 srcStageMask,
                                 srcAccessMask,
                                 dstStageMask,
                                 dstAccessMask,
                                 oldLayout,
                                 newLayout,
                                 srcQueueFamilyIndex,
                                 dstQueueFamilyIndex,
                                 image,
                                 {aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount}};
}

NODISCARD FORCEINLINE static VkBufferMemoryBarrier2 GetBufferMemoryBarrier(
    const VkBuffer& buffer, const VkDeviceSize size, const VkDeviceSize offset, const VkPipelineStageFlags2 srcStageMask,
    const VkAccessFlags2 srcAccessMask, const VkPipelineStageFlags2 dstStageMask, const VkAccessFlags2 dstAccessMask,
    const uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, const uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
{
    return VkBufferMemoryBarrier2{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                  nullptr,
                                  srcStageMask,
                                  srcAccessMask,
                                  dstStageMask,
                                  dstAccessMask,
                                  srcQueueFamilyIndex,
                                  dstQueueFamilyIndex,
                                  buffer,
                                  offset,
                                  size};
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

NODISCARD static VkFormat PathfinderShaderElementFormatToVulkan(const EShaderBufferElementType type)
{
    switch (type)
    {
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_INT: return VK_FORMAT_R32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT: return VK_FORMAT_R32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DOUBLE: return VK_FORMAT_R64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC2: return VK_FORMAT_R32G32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC2: return VK_FORMAT_R32G32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC2: return VK_FORMAT_R32G32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC2: return VK_FORMAT_R64G64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC3: return VK_FORMAT_R32G32B32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC3: return VK_FORMAT_R32G32B32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3: return VK_FORMAT_R32G32B32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC3: return VK_FORMAT_R64G64B64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC4: return VK_FORMAT_R32G32B32A32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC4: return VK_FORMAT_R32G32B32A32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC4: return VK_FORMAT_R64G64B64A64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT2:
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT3:
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT4:
        {
            PFR_ASSERT(false, "Unknown shader input element format!");
            return VK_FORMAT_UNDEFINED;
        }
    }

    PFR_ASSERT(false, "Unknown shader input element format!");
    return VK_FORMAT_UNDEFINED;
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

NODISCARD FORCEINLINE static VkDescriptorSetAllocateInfo GetDescriptorSetAllocateInfo(const VkDescriptorPool& descriptorPool,
                                                                                      const uint32_t descriptorSetCount,
                                                                                      const VkDescriptorSetLayout* descriptorSetLayouts)
{
    return VkDescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descriptorPool, descriptorSetCount,
                                       descriptorSetLayouts};
}

NODISCARD FORCEINLINE static VkDescriptorPoolCreateInfo GetDescriptorPoolCreateInfo(
    const uint32_t poolSizeCount, const uint32_t maxSetCount, const VkDescriptorPoolSize* poolSizes,
    const VkDescriptorPoolCreateFlags descriptorPoolCreateFlags = 0, const void* pNext = nullptr)
{
    return VkDescriptorPoolCreateInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, pNext, descriptorPoolCreateFlags, maxSetCount, poolSizeCount, poolSizes};
}

NODISCARD FORCEINLINE static VkPushConstantRange GetPushConstantRange(const VkShaderStageFlags stageFlags, const uint32_t offset,
                                                                      const uint32_t size)
{
    return VkPushConstantRange{stageFlags, offset, size};
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

NODISCARD static VkAccessFlags2 PathfinderAccessFlagsToVulkan(const RendererTypeFlags accessFlags)
{
    VkAccessFlags2 vkAccessFlags2 = VK_ACCESS_2_NONE;

    if (accessFlags & EAccessFlags::ACCESS_NONE) vkAccessFlags2 |= VK_ACCESS_2_NONE;
    if (accessFlags & EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_INDEX_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_INDEX_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_VERTEX_ATTRIBUTE_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_UNIFORM_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_UNIFORM_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_INPUT_ATTACHMENT_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_SHADER_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_SHADER_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_SHADER_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_SHADER_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_TRANSFER_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_TRANSFER_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_TRANSFER_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_HOST_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_HOST_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_HOST_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_HOST_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_MEMORY_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_MEMORY_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_MEMORY_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_MEMORY_WRITE_BIT;
    if (accessFlags & EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
    if (accessFlags & EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    if (accessFlags & EAccessFlags::ACCESS_VIDEO_DECODE_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_VIDEO_DECODE_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;

    if (accessFlags & EAccessFlags::ACCESS_VIDEO_ENCODE_READ_BIT) vkAccessFlags2 |= VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_VIDEO_ENCODE_WRITE_BIT) vkAccessFlags2 |= VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_ACCELERATION_STRUCTURE_READ_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    if (accessFlags & EAccessFlags::ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT)
        vkAccessFlags2 |= VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR;

    return vkAccessFlags2;
}

NODISCARD static VkPipelineStageFlags2 PathfinderPipelineStageToVulkan(const RendererTypeFlags pipelineStage)
{
    VkPipelineStageFlags2 vkPipelineStageFlags2 = 0;

    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_NONE) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_NONE;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_VERTEX_INPUT_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_HOST_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_HOST_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_ALL_COMMANDS_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_COPY_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_COPY_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_RESOLVE_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_BLIT_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_BLIT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_CLEAR_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_CLEAR_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_INDEX_INPUT_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_VERTEX_ATTRIBUTE_INPUT_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_PRE_RASTERIZATION_SHADERS_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_VIDEO_DECODE_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_VIDEO_ENCODE_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT) vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    if (pipelineStage & EPipelineStage::PIPELINE_STAGE_ACCELERATION_STRUCTURE_COPY_BIT)
        vkPipelineStageFlags2 |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;

    return vkPipelineStageFlags2;
}

NODISCARD FORCEINLINE static VkFilter PathfinderSamplerFilterToVulkan(const ESamplerFilter filter)
{
    switch (filter)
    {
        case ESamplerFilter::SAMPLER_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case ESamplerFilter::SAMPLER_FILTER_LINEAR: return VK_FILTER_LINEAR;
    }

    PFR_ASSERT(false, "Unknown sampler filter!");
    return VK_FILTER_LINEAR;
}

NODISCARD FORCEINLINE static VkSamplerAddressMode PathfinderSamplerWrapToVulkan(const ESamplerWrap wrap)
{
    switch (wrap)
    {
        case ESamplerWrap::SAMPLER_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case ESamplerWrap::SAMPLER_WRAP_MIRROR_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }

    PFR_ASSERT(false, "Unknown sampler wrap!");
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

}  // namespace VulkanUtility

}  // namespace Pathfinder
