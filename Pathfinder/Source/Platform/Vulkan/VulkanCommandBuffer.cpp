#include <PathfinderPCH.h>
#include "VulkanCommandBuffer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"

#include <Renderer/Renderer.h>

namespace Pathfinder
{

namespace CommandBufferUtils
{

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

NODISCARD FORCEINLINE static VkAttachmentLoadOp PathfinderLoadOpToVulkan(const EOp loadOp)
{
    switch (loadOp)
    {
        case EOp::DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case EOp::CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case EOp::LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    PFR_ASSERT(false, "Unknown load op!");
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

NODISCARD FORCEINLINE static VkAttachmentStoreOp PathfinderStoreOpToVulkan(const EOp storeOp)
{
    switch (storeOp)
    {
        case EOp::DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case EOp::STORE: return VK_ATTACHMENT_STORE_OP_STORE;
    }

    PFR_ASSERT(false, "Unknown store op!");
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

NODISCARD FORCEINLINE static VkShaderStageFlags PathfinderShaderStageFlagsToVulkan(const RendererTypeFlags shaderStageFlags)
{
    VkShaderStageFlags vkShaderStageFlags = 0;

    if (shaderStageFlags & EShaderStage::SHADER_STAGE_VERTEX) vkShaderStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL) vkShaderStageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION)
        vkShaderStageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_GEOMETRY) vkShaderStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_COMPUTE) vkShaderStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_FRAGMENT) vkShaderStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_TASK) vkShaderStageFlags |= VK_SHADER_STAGE_TASK_BIT_EXT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_MESH) vkShaderStageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_ALL_GRAPHICS) vkShaderStageFlags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_ALL) vkShaderStageFlags |= VK_SHADER_STAGE_ALL;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_RAYGEN) vkShaderStageFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_ANY_HIT) vkShaderStageFlags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_CLOSEST_HIT) vkShaderStageFlags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_MISS) vkShaderStageFlags |= VK_SHADER_STAGE_MISS_BIT_KHR;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_INTERSECTION) vkShaderStageFlags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    if (shaderStageFlags & EShaderStage::SHADER_STAGE_CALLABLE) vkShaderStageFlags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    PFR_ASSERT(vkShaderStageFlags > 0, "Shader stage flags can't be zero!");
    return vkShaderStageFlags;
}

}  // namespace CommandBufferUtils

VulkanQueryPool::VulkanQueryPool(const uint32_t queryCount, const bool bIsPipelineStatistics) : QueryPool(queryCount, bIsPipelineStatistics)
{
    const VkQueryPoolCreateInfo queryPoolCI = {
        .sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType          = bIsPipelineStatistics ? VK_QUERY_TYPE_PIPELINE_STATISTICS : VK_QUERY_TYPE_TIMESTAMP,
        .queryCount         = queryCount,
        .pipelineStatistics = bIsPipelineStatistics ? VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |  //
                                                          VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |  //
                                                          VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |  //
                                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |          //
                                                          VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |  //
                                                          VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT |
                                                          VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                                                          VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT
                                                    : VkQueryPipelineStatisticFlags{0}};
    VK_CHECK(vkCreateQueryPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &queryPoolCI, nullptr, &m_Handle),
             "Failed to create timestamp query pool!");
}

void VulkanQueryPool::Destroy()
{
    vkDestroyQueryPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, nullptr);
}

VulkanSyncPoint::VulkanSyncPoint(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages) noexcept
    : SyncPoint(timelineSemaphoreHandle, value, pipelineStages)
{
}

void VulkanSyncPoint::Wait() const noexcept
{
    const VkSemaphoreWaitInfo waitInfo = {.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                          .flags          = VK_SEMAPHORE_WAIT_ANY_BIT,
                                          .semaphoreCount = 1,
                                          .pSemaphores    = (VkSemaphore*)&m_TimelineSemaphoreHandle,
                                          .pValues        = &m_Value};

    VK_CHECK(vkWaitSemaphores(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &waitInfo, std::numeric_limits<uint64_t>::max()),
             "Failed to wait on SyncPoint's timeline semaphore!");
}

VulkanCommandBuffer::VulkanCommandBuffer(const CommandBufferSpecification& commandBufferSpec) : CommandBuffer(commandBufferSpec)
{
    const auto& context = VulkanContext::Get();
    context.GetDevice()->AllocateCommandBuffer(m_Handle, m_Specification);

    std::string commandBufferTypeStr;
    switch (m_Specification.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL: commandBufferTypeStr = "GRAPHICS"; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE_ASYNC: commandBufferTypeStr = "COMPUTE"; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER_ASYNC: commandBufferTypeStr = "TRANSFER"; break;
    }

    constexpr VkSemaphoreTypeCreateInfo semaphoreTypeCI = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE, .initialValue = 0};
    const VkSemaphoreCreateInfo semaphoreCI = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &semaphoreTypeCI};
    VK_CHECK(vkCreateSemaphore(context.GetDevice()->GetLogicalDevice(), &semaphoreCI, VK_NULL_HANDLE, &m_TimelineSemaphore.Handle),
             "Failed to create timeline semaphore!");

    const std::string semaphoreDebugName = "VK_TIMELINE_SEMAPHORE_" + commandBufferTypeStr;
    VK_SetDebugName(context.GetDevice()->GetLogicalDevice(), m_TimelineSemaphore.Handle, VK_OBJECT_TYPE_SEMAPHORE,
                    semaphoreDebugName.data());
}

void VulkanCommandBuffer::BeginRecording(bool bOneTimeSubmit, const void* inheritanceInfo)
{
    const VkSemaphoreWaitInfo swi = {.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                     .semaphoreCount = 1,
                                     .pSemaphores    = &m_TimelineSemaphore.Handle,
                                     .pValues        = &m_TimelineSemaphore.Counter};

    VK_CHECK(vkWaitSemaphores(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &swi, UINT64_MAX),
             "Failed to wait on timeline semaphore!");

    VkCommandBufferBeginInfo commandBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                       .pInheritanceInfo =
                                                           static_cast<const VkCommandBufferInheritanceInfo*>(inheritanceInfo)};
    if (m_Specification.Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        PFR_ASSERT(inheritanceInfo, "Secondary command buffer requires valid inheritance info!");
    }

    if (bOneTimeSubmit) commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(m_Handle, &commandBufferBeginInfo), "Failed to begin command buffer recording!");
}

void VulkanCommandBuffer::EndRecording() const
{
    VK_CHECK(vkEndCommandBuffer(m_Handle), "Failed to end recording command buffer");
}

Shared<SyncPoint> VulkanCommandBuffer::Submit(const std::vector<Shared<SyncPoint>>& waitPoints,
                                              const std::vector<Shared<SyncPoint>>& signalPoints, const void* signalFence)
{
    VkSubmitInfo2 submitInfo2 = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    ++m_TimelineSemaphore.Counter;

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreInfos(waitPoints.size());
    for (size_t i{}; i < waitPoints.size(); ++i)
    {
        waitSemaphoreInfos[i].sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfos[i].semaphore   = (VkSemaphore)waitPoints[i]->GetTimelineSemaphore();
        waitSemaphoreInfos[i].value       = waitPoints[i]->GetValue();
        waitSemaphoreInfos[i].stageMask   = CommandBufferUtils::PathfinderPipelineStageToVulkan(waitPoints[i]->GetPipelineStages());
        waitSemaphoreInfos[i].deviceIndex = 0;
    }
    submitInfo2.pWaitSemaphoreInfos    = waitSemaphoreInfos.data();
    submitInfo2.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfos.size());

    auto& context                    = VulkanContext::Get();
    VkQueue queue                    = VK_NULL_HANDLE;
    RendererTypeFlags pipelineStages = EPipelineStage::PIPELINE_STAGE_NONE;
    switch (m_Specification.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL:
        {
            queue = context.GetDevice()->GetGraphicsQueue();
            pipelineStages |= EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT | EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                              EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE_ASYNC:
        {
            queue = context.GetDevice()->GetComputeQueue();
            pipelineStages |= EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT | EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER_ASYNC:
        {
            queue = context.GetDevice()->GetTransferQueue();
            pipelineStages |= EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            break;
        }
    }

    // +1 for current submission
    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreInfos(signalPoints.size() + 1);
    const auto currentSyncPoint = SyncPoint::Create(m_TimelineSemaphore.Handle, m_TimelineSemaphore.Counter, pipelineStages);
    signalSemaphoreInfos[signalPoints.size()].sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfos[signalPoints.size()].semaphore = (VkSemaphore)currentSyncPoint->GetTimelineSemaphore();
    signalSemaphoreInfos[signalPoints.size()].value     = currentSyncPoint->GetValue();
    signalSemaphoreInfos[signalPoints.size()].stageMask =
        CommandBufferUtils::PathfinderPipelineStageToVulkan(currentSyncPoint->GetPipelineStages());
    signalSemaphoreInfos[signalPoints.size()].deviceIndex = 0;

    for (size_t i{}; i < signalPoints.size(); ++i)
    {
        signalSemaphoreInfos[i].sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfos[i].semaphore   = (VkSemaphore)signalPoints[i]->GetTimelineSemaphore();
        signalSemaphoreInfos[i].value       = signalPoints[i]->GetValue();
        signalSemaphoreInfos[i].stageMask   = CommandBufferUtils::PathfinderPipelineStageToVulkan(signalPoints[i]->GetPipelineStages());
        signalSemaphoreInfos[i].deviceIndex = 0;
    }
    submitInfo2.pSignalSemaphoreInfos    = signalSemaphoreInfos.data();
    submitInfo2.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());

    const VkCommandBufferSubmitInfo commandBufferSI = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = m_Handle};
    submitInfo2.commandBufferInfoCount              = 1;
    submitInfo2.pCommandBufferInfos                 = &commandBufferSI;

    // NOTE: Since fence is OPTIONAL && I use timeline semaphores, so I can wait on them on host. But for swapchain we use 'RenderFence'.
    VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo2, (VkFence)signalFence), "Failed submit command buffer!");

    return currentSyncPoint;
}

void VulkanCommandBuffer::TransitionImageLayout(const VkImage& image, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                                const VkImageAspectFlags aspectMask, const uint32_t layerCount, const uint32_t baseLayer,
                                                const uint32_t mipLevels, const uint32_t baseMipLevel) const
{
    if (oldLayout == newLayout) return;

    VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE;
    VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE;

    VkPipelineStageFlags2 srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    VkPipelineStageFlags2 dstStageMask = VK_PIPELINE_STAGE_2_NONE;

    // Determine stages and resource access:
    switch (oldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
        {
            srcAccessMask = 0;
            srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_GENERAL:  // NOTE: May be not optimal stage
        {
            srcAccessMask = 0;
            srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            srcStageMask =
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;  // render target and storage image case?
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            break;
        }
        default: PFR_ASSERT(false, "Old layout is not supported!");
    }

    switch (newLayout)
    {
        case VK_IMAGE_LAYOUT_GENERAL:
        {
            dstAccessMask = 0;  // GENERAL layout means resource can be affected by any operations
            dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            dstStageMask =
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;  // render target and storage image case?
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        default: PFR_ASSERT(false, "New layout is not supported!");
    }

    const auto vkSrcPipelineStages = CommandBufferUtils::PathfinderPipelineStageToVulkan(srcStageMask);
    const auto vkDstPipelineStages = CommandBufferUtils::PathfinderPipelineStageToVulkan(dstStageMask);

    const auto imageMemoryBarrier2 = VulkanUtils::GetImageMemoryBarrier(
        image, aspectMask, oldLayout, newLayout, vkSrcPipelineStages, CommandBufferUtils::PathfinderAccessFlagsToVulkan(srcAccessMask),
        vkDstPipelineStages, CommandBufferUtils::PathfinderAccessFlagsToVulkan(dstAccessMask), layerCount, baseLayer, mipLevels,
        baseMipLevel);

    InsertBarrier(vkSrcPipelineStages, vkDstPipelineStages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier2);
}

void VulkanCommandBuffer::BeginRendering(const std::vector<Shared<Texture>>& attachments,
                                         const std::vector<RenderingInfo>& renderingInfos) const
{
    PFR_ASSERT(!attachments.empty(), "Nothing to render into!");
    PFR_ASSERT(renderingInfos.size() == attachments.size(), "Rendering Infos should equal to attachments count!");

    VkRenderingInfo renderingInfo = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO};
    if (m_Specification.Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
        renderingInfo.flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    uint32_t maxLayerCount = 1;
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    VkRenderingAttachmentInfo depthAttachment   = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingAttachmentInfo stencilAttachment = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};

    // TODO: Stencil
    for (size_t i{}; i < attachments.size(); ++i)
    {
        maxLayerCount = std::max(maxLayerCount, attachments[i]->GetSpecification().Layers);

        if (TextureUtils::IsDepthFormat(attachments[i]->GetSpecification().Format))
        {
            const auto* color = std::get_if<DepthStencilClearValue>(&renderingInfos[i].ClearValue);
            PFR_ASSERT(color, "DepthStencilClearValue is not valid!");

            depthAttachment.imageView               = (VkImageView)attachments[i]->GetView();
            depthAttachment.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp                  = CommandBufferUtils::PathfinderLoadOpToVulkan(renderingInfos[i].LoadOp);
            depthAttachment.storeOp                 = CommandBufferUtils::PathfinderStoreOpToVulkan(renderingInfos[i].StoreOp);
            depthAttachment.clearValue.depthStencil = {.depth = color->Depth, .stencil = color->Stencil};
        }
        else
        {
            const auto* color = std::get_if<ColorClearValue>(&renderingInfos[i].ClearValue);
            PFR_ASSERT(color, "ColorClearValue is not valid!");

            auto& attachment            = colorAttachments.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            attachment.imageView        = (VkImageView)attachments[i]->GetView();
            attachment.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp           = CommandBufferUtils::PathfinderLoadOpToVulkan(renderingInfos[i].LoadOp);
            attachment.storeOp          = CommandBufferUtils::PathfinderStoreOpToVulkan(renderingInfos[i].StoreOp);
            attachment.clearValue.color = {color->x, color->y, color->z, color->w};
        }
    }

    // NOTE: Assuming all attachments have the same dimensions.
    renderingInfo.renderArea = {
        .offset{.x = 0, .y = 0},
        .extent{.width = attachments[0]->GetSpecification().Dimensions.x, .height = attachments[0]->GetSpecification().Dimensions.y}};
    renderingInfo.layerCount = maxLayerCount;

    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments    = colorAttachments.data();

    if (depthAttachment.imageView) renderingInfo.pDepthAttachment = &depthAttachment;
    if (stencilAttachment.imageView) renderingInfo.pStencilAttachment = &stencilAttachment;

    BeginRendering(&renderingInfo);
}

void VulkanCommandBuffer::InsertBarrier(const VkPipelineStageFlags2 srcStageMask, const VkPipelineStageFlags2 dstStageMask,
                                        const VkDependencyFlags dependencyFlags, const uint32_t memoryBarrierCount,
                                        const VkMemoryBarrier2* pMemoryBarriers, const uint32_t bufferMemoryBarrierCount,
                                        const VkBufferMemoryBarrier2* pBufferMemoryBarriers, const uint32_t imageMemoryBarrierCount,
                                        const VkImageMemoryBarrier2* pImageMemoryBarriers) const
{
    Renderer::GetStats().BarrierCount += memoryBarrierCount + bufferMemoryBarrierCount + imageMemoryBarrierCount;
    ++Renderer::GetStats().BarrierBatchCount;
    const VkDependencyInfo dependencyInfo = {.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                             .dependencyFlags          = 0,
                                             .memoryBarrierCount       = memoryBarrierCount,
                                             .pMemoryBarriers          = pMemoryBarriers,
                                             .bufferMemoryBarrierCount = bufferMemoryBarrierCount,
                                             .pBufferMemoryBarriers    = pBufferMemoryBarriers,
                                             .imageMemoryBarrierCount  = imageMemoryBarrierCount,
                                             .pImageMemoryBarriers     = pImageMemoryBarriers};

    vkCmdPipelineBarrier2(m_Handle, &dependencyInfo);
}

void VulkanCommandBuffer::BindPushConstants(Shared<Pipeline> pipeline, const uint32_t offset, const uint32_t size, const void* data) const
{
    auto vulkanPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    PFR_ASSERT(vulkanPipeline, "Failed to cast Pipeline to VulkanPipeline!");
    vkCmdPushConstants(m_Handle, vulkanPipeline->GetLayout(), VK_SHADER_STAGE_ALL, offset, size, data);
}

void VulkanCommandBuffer::Destroy()
{
    if (!m_Handle) return;

    auto& context = VulkanContext::Get();
    context.GetDevice()->FreeCommandBuffer(m_Handle, m_Specification);

    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    vkDestroySemaphore(logicalDevice, m_TimelineSemaphore.Handle, VK_NULL_HANDLE);

    m_Handle = VK_NULL_HANDLE;
}

void VulkanCommandBuffer::SetViewportAndScissor(const uint32_t width, const uint32_t height, const int32_t offsetX,
                                                const int32_t offsetY) const
{
    const VkViewport viewport = {.x        = 0,
                                 .y        = static_cast<float>(height),   //
                                 .width    = static_cast<float>(width),    //
                                 .height   = -static_cast<float>(height),  //
                                 .minDepth = 0.f,
                                 .maxDepth = 1.f};
    vkCmdSetViewport(m_Handle, 0, 1, &viewport);

    const VkRect2D scissor = {.offset{.x = offsetX, .y = offsetY}, .extent{.width = width, .height = height}};
    vkCmdSetScissor(m_Handle, 0, 1, &scissor);
}

void VulkanCommandBuffer::BindPipeline(Shared<Pipeline>& pipeline) const
{
    if (pipeline->GetSpecification().PipelineType == EPipelineType::PIPELINE_TYPE_GRAPHICS)
    {
        const auto& gpo = pipeline->GetPipelineOptions<GraphicsPipelineOptions>();

        if (gpo.bDynamicPolygonMode) vkCmdSetPolygonModeEXT(m_Handle, VulkanUtils::PathfinderPolygonModeToVulkan(gpo.PolygonMode));
    }

    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
        case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
    }

    if (pipeline->GetSpecification().PipelineType == EPipelineType::PIPELINE_TYPE_GRAPHICS)
    {
        const auto& gpo = pipeline->GetPipelineOptions<GraphicsPipelineOptions>();

        if (gpo.LineWidth != 1.f) vkCmdSetLineWidth(m_Handle, gpo.LineWidth);
    }

    vkCmdBindPipeline(m_Handle, pipelineBindPoint, (VkPipeline)pipeline->Get());
}

FORCEINLINE void VulkanCommandBuffer::DrawIndexedIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                                          const uint32_t stride) const
{
    PFR_ASSERT(drawBuffer && drawBuffer->Get(), "Invalid draw buffer!");

    vkCmdDrawIndexedIndirect(m_Handle, (VkBuffer)drawBuffer->Get(), offset, drawCount, stride);
}

void VulkanCommandBuffer::BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding,
                                            const uint32_t bindingCount, const uint64_t* offsets) const
{
    std::vector<VkBuffer> vulkanBuffers;
    vulkanBuffers.reserve(vertexBuffers.size());

    for (auto& vertexBuffer : vertexBuffers)
    {
        auto vulkanVertexBuffer = std::static_pointer_cast<VulkanBuffer>(vertexBuffer);
        PFR_ASSERT(vulkanVertexBuffer, "Failed to cast Buffer to VulkanBuffer!");

        vulkanBuffers.emplace_back((VkBuffer)vulkanVertexBuffer->Get());
    }

    vkCmdBindVertexBuffers(m_Handle, firstBinding, bindingCount, vulkanBuffers.data(), offsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset, bool bIndexType32) const
{
    auto vulkanIndexBuffer = std::static_pointer_cast<VulkanBuffer>(indexBuffer);
    PFR_ASSERT(vulkanIndexBuffer, "Failed to cast Buffer to VulkanBuffer!");

    vkCmdBindIndexBuffer(m_Handle, (VkBuffer)vulkanIndexBuffer->Get(), offset, bIndexType32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void VulkanCommandBuffer::DrawMeshTasksIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset, const uint32_t drawCount,
                                                const uint32_t stride) const
{
    PFR_ASSERT(drawBuffer && drawBuffer->Get(), "Invalid draw buffer!");
    vkCmdDrawMeshTasksIndirectEXT(m_Handle, (VkBuffer)drawBuffer->Get(), offset, drawCount, stride);
}

void VulkanCommandBuffer::DrawMeshTasksMultiIndirect(const Shared<Buffer>& drawBuffer, const uint64_t offset,
                                                     const Shared<Buffer>& countBuffer, const uint64_t countBufferOffset,
                                                     const uint32_t maxDrawCount, const uint32_t stride) const
{
    PFR_ASSERT(drawBuffer && drawBuffer->Get() && countBuffer && countBuffer->Get(), "Invalid draw/count buffer!");
    vkCmdDrawMeshTasksIndirectCountEXT(m_Handle, (VkBuffer)drawBuffer->Get(), offset, (VkBuffer)countBuffer->Get(), countBufferOffset,
                                       maxDrawCount, stride);
}

void VulkanCommandBuffer::FillBuffer(const Shared<Buffer>& buffer, const uint32_t data) const
{
    const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    const auto descriptorInfo = vulkanBuffer->GetDescriptorInfo();
    vkCmdFillBuffer(m_Handle, (VkBuffer)vulkanBuffer->Get(), descriptorInfo.offset, descriptorInfo.range, data);
}

void VulkanCommandBuffer::InsertBarriers(const std::vector<MemoryBarrier>& memoryBarriers,
                                         const std::vector<BufferMemoryBarrier>& bufferMemoryBarriers,
                                         const std::vector<ImageMemoryBarrier>& imageMemoryBarriers) const
{
    Renderer::GetStats().BarrierCount += memoryBarriers.size() + bufferMemoryBarriers.size() + imageMemoryBarriers.size();
    ++Renderer::GetStats().BarrierBatchCount;

    std::vector<VkMemoryBarrier2> memoryBarriersVK(memoryBarriers.size());
    for (uint32_t i{}; i < memoryBarriers.size(); ++i)
    {
        auto& memoryBarrier = memoryBarriers[i];
        memoryBarriersVK[i] = {.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                               .srcStageMask  = CommandBufferUtils::PathfinderPipelineStageToVulkan(memoryBarrier.srcStageMask),
                               .srcAccessMask = CommandBufferUtils::PathfinderAccessFlagsToVulkan(memoryBarrier.srcAccessMask),
                               .dstStageMask  = CommandBufferUtils::PathfinderPipelineStageToVulkan(memoryBarrier.dstStageMask),
                               .dstAccessMask = CommandBufferUtils::PathfinderAccessFlagsToVulkan(memoryBarrier.dstAccessMask)};
    }

    std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriersVK(bufferMemoryBarriers.size());
    for (uint32_t i{}; i < bufferMemoryBarriers.size(); ++i)
    {
        auto& bufferMemoryBarrier = bufferMemoryBarriers[i];
        bufferMemoryBarriersVK[i] = {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask        = CommandBufferUtils::PathfinderPipelineStageToVulkan(bufferMemoryBarrier.srcStageMask),
            .srcAccessMask       = CommandBufferUtils::PathfinderAccessFlagsToVulkan(bufferMemoryBarrier.srcAccessMask),
            .dstStageMask        = CommandBufferUtils::PathfinderPipelineStageToVulkan(bufferMemoryBarrier.dstStageMask),
            .dstAccessMask       = CommandBufferUtils::PathfinderAccessFlagsToVulkan(bufferMemoryBarrier.dstAccessMask),
            .srcQueueFamilyIndex = bufferMemoryBarrier.srcQueueFamilyIndex.has_value() ? bufferMemoryBarrier.srcQueueFamilyIndex.value()
                                                                                       : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = bufferMemoryBarrier.dstQueueFamilyIndex.has_value() ? bufferMemoryBarrier.dstQueueFamilyIndex.value()
                                                                                       : VK_QUEUE_FAMILY_IGNORED,
            .buffer              = (VkBuffer)bufferMemoryBarrier.buffer->Get(),
            .offset              = 0,
            .size                = bufferMemoryBarrier.buffer->GetSpecification().Capacity};
    }

    std::vector<VkImageMemoryBarrier2> imageMemoryBarriersVK(imageMemoryBarriers.size());
    for (uint32_t i{}; i < imageMemoryBarriers.size(); ++i)
    {
        auto& imageMemoryBarrier = imageMemoryBarriers[i];

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_NONE;
        if (TextureUtils::IsDepthFormat(imageMemoryBarrier.texture->GetSpecification().Format))
        {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (TextureUtils::IsStencilFormat(imageMemoryBarrier.texture->GetSpecification().Format))
                aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        imageMemoryBarriersVK[i] = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask        = CommandBufferUtils::PathfinderPipelineStageToVulkan(imageMemoryBarrier.srcStageMask),
            .srcAccessMask       = CommandBufferUtils::PathfinderAccessFlagsToVulkan(imageMemoryBarrier.srcAccessMask),
            .dstStageMask        = CommandBufferUtils::PathfinderPipelineStageToVulkan(imageMemoryBarrier.dstStageMask),
            .dstAccessMask       = CommandBufferUtils::PathfinderAccessFlagsToVulkan(imageMemoryBarrier.dstAccessMask),
            .oldLayout           = TextureUtils::PathfinderImageLayoutToVulkan(imageMemoryBarrier.oldLayout),
            .newLayout           = TextureUtils::PathfinderImageLayoutToVulkan(imageMemoryBarrier.newLayout),
            .srcQueueFamilyIndex = imageMemoryBarrier.srcQueueFamilyIndex.has_value() ? imageMemoryBarrier.srcQueueFamilyIndex.value()
                                                                                      : VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = imageMemoryBarrier.dstQueueFamilyIndex.has_value() ? imageMemoryBarrier.dstQueueFamilyIndex.value()
                                                                                      : VK_QUEUE_FAMILY_IGNORED,
            .image               = (VkImage)imageMemoryBarrier.texture->Get(),
            .subresourceRange    = {.aspectMask     = aspectMask,
                                    .baseMipLevel   = imageMemoryBarrier.subresourceRange.baseMipLevel,
                                    .levelCount     = imageMemoryBarrier.subresourceRange.mipCount,
                                    .baseArrayLayer = imageMemoryBarrier.subresourceRange.baseArrayLayer,
                                    .layerCount     = imageMemoryBarrier.subresourceRange.layerCount}};
    }

    const VkDependencyInfo dependencyInfo = {.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                             .dependencyFlags          = 0,
                                             .memoryBarrierCount       = static_cast<uint32_t>(memoryBarriersVK.size()),
                                             .pMemoryBarriers          = memoryBarriersVK.data(),
                                             .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferMemoryBarriersVK.size()),
                                             .pBufferMemoryBarriers    = bufferMemoryBarriersVK.data(),
                                             .imageMemoryBarrierCount  = static_cast<uint32_t>(imageMemoryBarriersVK.size()),
                                             .pImageMemoryBarriers     = imageMemoryBarriersVK.data()};

    vkCmdPipelineBarrier2(m_Handle, &dependencyInfo);
}

void VulkanCommandBuffer::BeginPipelineStatisticsQuery(Shared<QueryPool>& queryPool)
{
    vkCmdBeginQuery(m_Handle, (VkQueryPool)queryPool->Get(), 0, 0);
}

void VulkanCommandBuffer::EndPipelineStatisticsQuery(Shared<QueryPool>& queryPool)
{
    vkCmdEndQuery(m_Handle, (VkQueryPool)queryPool->Get(), 0);
}

std::vector<std::pair<std::string, std::uint64_t>> VulkanCommandBuffer::CalculateQueryPoolStatisticsResults(Shared<QueryPool>& queryPool)
{
    static constexpr uint32_t s_MAX_PIPELINE_STATISITCS                                            = 13;
    static std::array<const std::string_view, s_MAX_PIPELINE_STATISITCS> s_PipelineStatisticsNames = {
        "Input assembly vertex count        ",  // IA vertex
        "Input assembly primitives count    ",  // IA primitives
        "Vertex shader invocations          ",  // VS
        "Geometry shader invocations        ",  // GS
        "Geometry shader primitives         ",  //
        "Clipping stage primitives processed",  // Clipping
        "Clipping stage primitives output   ",  //
        "Fragment shader invocations        ",  // FS
        "Tess. Control shader invocations   ",  // TCS
        "Tess. Evaluation shader invocations",  // TES
        "Compute shader invocations         ",  // CSCS
        "Task shader invocations            ",  // TS
        "Mesh shader invocations            "   // MS
    };

    std::vector<std::pair<std::string, std::uint64_t>> results(s_MAX_PIPELINE_STATISITCS);
    if (queryPool->Get())
    {
        std::vector<uint64_t> pipelineStatistiscs(s_MAX_PIPELINE_STATISITCS, 0);
        const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
        VK_CHECK(vkGetQueryPoolResults(logicalDevice, (VkQueryPool)queryPool->Get(), 0, queryPool->GetQueryCount(),
                                       static_cast<uint32_t>(pipelineStatistiscs.size() * sizeof(pipelineStatistiscs[0])),
                                       pipelineStatistiscs.data(),
                                       static_cast<uint32_t>(pipelineStatistiscs.size() * sizeof(pipelineStatistiscs[0])),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
                 "Failed to retrieve pipeline statistics query results!");

        for (size_t i = 0; i < pipelineStatistiscs.size(); ++i)
        {
            results[i].first  = s_PipelineStatisticsNames[i];
            results[i].second = pipelineStatistiscs[i];
        }
    }

    return results;
}

std::vector<uint64_t> VulkanCommandBuffer::CalculateQueryPoolProfilerResults(Shared<QueryPool>& queryPool, const size_t timestampCount)
{
    std::vector<uint64_t> results(timestampCount);

    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
    VK_CHECK(vkGetQueryPoolResults(logicalDevice, (VkQueryPool)queryPool->Get(), 0, timestampCount,
                                   static_cast<uint32_t>(results.size() * sizeof(results[0])), results.data(),
                                   static_cast<uint32_t>(sizeof(results[0])), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
             "Failed to retrieve timestamps query results!");

    return results;
}

void VulkanCommandBuffer::ResetPool(Shared<QueryPool>& queryPool)
{
    vkCmdResetQueryPool(m_Handle, (VkQueryPool)queryPool->Get(), 0, queryPool->GetQueryCount());
}

void VulkanCommandBuffer::WriteTimestamp(Shared<QueryPool>& queryPool, const uint32_t timestampIndex, const EPipelineStage pipelineStage)
{
    auto vulkanQueryPool = std::static_pointer_cast<VulkanQueryPool>(queryPool);
    PFR_ASSERT(vulkanQueryPool, "Failed to cast QueryPool to VulkanQueryPool");

    vkCmdWriteTimestamp(m_Handle, (VkPipelineStageFlagBits)CommandBufferUtils::PathfinderPipelineStageToVulkan(pipelineStage),
                        (VkQueryPool)vulkanQueryPool->Get(), timestampIndex);
}

}  // namespace Pathfinder