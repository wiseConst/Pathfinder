#include "PathfinderPCH.h"
#include "VulkanCommandBuffer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanPipeline.h"
#include "VulkanImage.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"

#include "Core/Application.h"
#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/Framebuffer.h"

namespace Pathfinder
{

static VkShaderStageFlags PathfinderShaderStageFlagsToVulkan(const RendererTypeFlags shaderStageFlags)
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

VulkanSyncPoint::VulkanSyncPoint(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages)
    : SyncPoint(timelineSemaphoreHandle, value, pipelineStages)
{
}

void VulkanSyncPoint::Wait() const
{
    const VkSemaphoreWaitInfo waitInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,    nullptr, VK_SEMAPHORE_WAIT_ANY_BIT, 1,
                                          (VkSemaphore*)&m_TimelineSemaphoreHandle, &m_Value};

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
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS: commandBufferTypeStr = "GRAPHICS"; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE: commandBufferTypeStr = "COMPUTE"; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER: commandBufferTypeStr = "TRANSFER"; break;
    }

    {
        constexpr VkSemaphoreTypeCreateInfo semaphoreTypeCI = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                                               VK_SEMAPHORE_TYPE_TIMELINE, 0};
        VkSemaphoreCreateInfo semaphoreCI                   = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &semaphoreTypeCI};
        VK_CHECK(vkCreateSemaphore(context.GetDevice()->GetLogicalDevice(), &semaphoreCI, VK_NULL_HANDLE, &m_TimelineSemaphore.Handle),
                 "Failed to create timeline semaphore!");

        const std::string semaphoreDebugName = "VK_TIMELINE_SEMAPHORE_" + commandBufferTypeStr;
        VK_SetDebugName(context.GetDevice()->GetLogicalDevice(), m_TimelineSemaphore.Handle, VK_OBJECT_TYPE_SEMAPHORE,
                        semaphoreDebugName.data());
    }
}

const std::vector<float> VulkanCommandBuffer::GetTimestampsResults()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
    if (m_CurrentTimestampIndex != 0)
    {
        VK_CHECK(vkGetQueryPoolResults(logicalDevice, m_TimestampQuery, 0, m_CurrentTimestampIndex,
                                       static_cast<uint32_t>(m_TimestampsResults.size() * sizeof(m_TimestampsResults[0])),
                                       m_TimestampsResults.data(), static_cast<uint32_t>(sizeof(m_TimestampsResults[0])),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
                 "Failed to retrieve timestamps query results!");
    }

    if (m_PipelineStatisticsQuery)
    {
        std::vector<uint64_t> pipelineStatistiscs(m_PipelineStatisticsResults.size(), 0);
        VK_CHECK(vkGetQueryPoolResults(
                     logicalDevice, m_PipelineStatisticsQuery, 0, 1,
                     static_cast<uint32_t>(pipelineStatistiscs.size() * sizeof(pipelineStatistiscs[0])), pipelineStatistiscs.data(),
                     static_cast<uint32_t>(pipelineStatistiscs.size() * sizeof(pipelineStatistiscs[0])), VK_QUERY_RESULT_64_BIT),
                 "Failed to retrieve pipeline statistics query results!");

        for (size_t i = 0; i < pipelineStatistiscs.size(); ++i)
        {
            m_PipelineStatisticsResults[i].first  = static_cast<EQueryPipelineStatistic>(BIT(i));
            m_PipelineStatisticsResults[i].second = pipelineStatistiscs[i];
        }
    }

    std::vector<float> result(s_MAX_TIMESTAMPS / 2);
    if (m_CurrentTimestampIndex == 0) return result;

    // NOTE: timestampPeriod contains the number of nanoseconds it takes for a timestamp query value to be increased by 1("tick").
    const float timestampPeriod = VulkanContext::Get().GetDevice()->GetTimestampPeriod();

    uint16_t k = 0;
    for (size_t i = 0; i < m_TimestampsResults.size() - 1; i += 2)
    {
        result[k] = static_cast<float>(m_TimestampsResults[i + 1] - m_TimestampsResults[i]) * timestampPeriod /
                    1000000.0f;  // convert nanoseconds to milliseconds
        ++k;
    }

    return result;
}

void VulkanCommandBuffer::BeginPipelineStatisticsQuery()
{
    if (!m_PipelineStatisticsQuery)
    {
        VkQueryPoolCreateInfo queryPoolCI = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_PIPELINE_STATISTICS, 1};
        if (m_Specification.Type == ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS)
        {
            queryPoolCI.pipelineStatistics =
                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |  //
                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |                                                            //
                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |                                                  //
                VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |  //
                VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |                                                 //
                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
                VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT;
        }
        else if (m_Specification.Type == ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE)
        {
            queryPoolCI.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
        }

        VK_CHECK(vkCreateQueryPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &queryPoolCI, nullptr, &m_PipelineStatisticsQuery),
                 "Failed to create pipeline statistics query pool!");
    }

    vkCmdResetQueryPool(m_Handle, m_PipelineStatisticsQuery, 0, 1);
    vkCmdBeginQuery(m_Handle, m_PipelineStatisticsQuery, 0, 0);
}

void VulkanCommandBuffer::BeginTimestampQuery(const EPipelineStage pipelineStage)
{
    PFR_ASSERT(m_CurrentTimestampIndex + 1 < s_MAX_TIMESTAMPS, "Timestamp query limit reached!");

    if (!m_TimestampQuery)
    {
        constexpr VkQueryPoolCreateInfo queryPoolCI = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP,
                                                       s_MAX_TIMESTAMPS};
        VK_CHECK(vkCreateQueryPool(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &queryPoolCI, nullptr, &m_TimestampQuery),
                 "Failed to create timestamp query pool!");
    }

    if (m_CurrentTimestampIndex == 0) vkCmdResetQueryPool(m_Handle, m_TimestampQuery, m_CurrentTimestampIndex, s_MAX_TIMESTAMPS);
    vkCmdWriteTimestamp(m_Handle, (VkPipelineStageFlagBits)VulkanUtility::PathfinderPipelineStageToVulkan(pipelineStage), m_TimestampQuery,
                        m_CurrentTimestampIndex);
    ++m_CurrentTimestampIndex;
}

void VulkanCommandBuffer::InsertExecutionBarrier(const RendererTypeFlags srcPipelineStages, const RendererTypeFlags srcAccessFlags,
                                                 const RendererTypeFlags dstPipelineStages, const RendererTypeFlags dstAccessFlags) const
{
    ++Renderer::GetStats().BarrierCount;
    const VkMemoryBarrier2 memoryBarrier2 = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                             nullptr,
                                             VulkanUtility::PathfinderPipelineStageToVulkan(srcPipelineStages),
                                             VulkanUtility::PathfinderAccessFlagsToVulkan(srcAccessFlags),
                                             VulkanUtility::PathfinderPipelineStageToVulkan(dstPipelineStages),
                                             VulkanUtility::PathfinderAccessFlagsToVulkan(dstAccessFlags)};

    const VkDependencyInfo dependencyInfo = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, VK_DEPENDENCY_BY_REGION_BIT, 1, &memoryBarrier2, 0, nullptr, 0, nullptr};
    vkCmdPipelineBarrier2(m_Handle, &dependencyInfo);
}

void VulkanCommandBuffer::InsertBufferMemoryBarrier(const Shared<Buffer> buffer, const RendererTypeFlags srcPipelineStages,
                                                    const RendererTypeFlags dstPipelineStages, const RendererTypeFlags srcAccessFlags,
                                                    const RendererTypeFlags dstAccessFlags) const
{
    const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    const auto vkSrcAccessFlags    = VulkanUtility::PathfinderAccessFlagsToVulkan(srcAccessFlags);
    const auto vkDstAccessFlags    = VulkanUtility::PathfinderAccessFlagsToVulkan(dstAccessFlags);
    const auto vkSrcPipelineStages = VulkanUtility::PathfinderPipelineStageToVulkan(srcPipelineStages);
    const auto vkDstPipelineStages = VulkanUtility::PathfinderPipelineStageToVulkan(dstPipelineStages);

    const VkBuffer rawBuffer = (VkBuffer)vulkanBuffer->Get();
    const auto bufferMemoryBarrier2 =
        VulkanUtility::GetBufferMemoryBarrier(rawBuffer, vulkanBuffer->GetSpecification().Capacity, 0, vkSrcPipelineStages,
                                              vkSrcAccessFlags, vkDstPipelineStages, vkDstAccessFlags);
    InsertBarrier(vkSrcPipelineStages, vkDstPipelineStages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &bufferMemoryBarrier2, 0, nullptr);
}

void VulkanCommandBuffer::BeginRecording(bool bOneTimeSubmit, const void* inheritanceInfo)
{
    {
        VkSemaphoreWaitInfo swi = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        swi.pSemaphores         = &m_TimelineSemaphore.Handle;
        swi.semaphoreCount      = 1;
        swi.pValues             = &m_TimelineSemaphore.Counter;

        VK_CHECK(vkWaitSemaphores(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &swi, UINT64_MAX),
                 "Failed to wait on timeline semaphore!");
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (m_Specification.Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        PFR_ASSERT(inheritanceInfo, "Secondary command buffer requires valid inheritance info!");
    }

    if (bOneTimeSubmit) commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    commandBufferBeginInfo.pInheritanceInfo = static_cast<const VkCommandBufferInheritanceInfo*>(inheritanceInfo);
    VK_CHECK(vkBeginCommandBuffer(m_Handle, &commandBufferBeginInfo), "Failed to begin command buffer recording!");

    if (m_Specification.Type == ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER) return;

    m_CurrentTimestampIndex = 0;
}

Shared<SyncPoint> VulkanCommandBuffer::Submit(const std::vector<Shared<SyncPoint>>& waitPoints,
                                              const std::vector<Shared<SyncPoint>>& signalPoints, const void* signalFence)
{
    VkSubmitInfo2 submitInfo2 = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    ++m_TimelineSemaphore.Counter;

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreInfos(waitPoints.size());
    for (size_t i{}; i < waitPoints.size(); ++i)
    {
        waitSemaphoreInfos[i].sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfos[i].semaphore   = (VkSemaphore)waitPoints[i]->GetTimelineSemaphore();
        waitSemaphoreInfos[i].value       = waitPoints[i]->GetValue();
        waitSemaphoreInfos[i].stageMask   = VulkanUtility::PathfinderPipelineStageToVulkan(waitPoints[i]->GetPipelineStages());
        waitSemaphoreInfos[i].deviceIndex = 0;
    }
    submitInfo2.pWaitSemaphoreInfos    = waitSemaphoreInfos.data();
    submitInfo2.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfos.size());

    auto& context                    = VulkanContext::Get();
    VkQueue queue                    = VK_NULL_HANDLE;
    RendererTypeFlags pipelineStages = EPipelineStage::PIPELINE_STAGE_NONE;
    switch (m_Specification.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS:
        {
            queue = context.GetDevice()->GetGraphicsQueue();
            pipelineStages |= EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT | EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                              EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE:
        {
            queue = context.GetDevice()->GetComputeQueue();
            pipelineStages |= EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT | EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER:
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
        VulkanUtility::PathfinderPipelineStageToVulkan(currentSyncPoint->GetPipelineStages());
    signalSemaphoreInfos[signalPoints.size()].deviceIndex = 0;

    for (size_t i{}; i < signalPoints.size(); ++i)
    {
        signalSemaphoreInfos[i].sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfos[i].semaphore   = (VkSemaphore)signalPoints[i]->GetTimelineSemaphore();
        signalSemaphoreInfos[i].value       = signalPoints[i]->GetValue();
        signalSemaphoreInfos[i].stageMask   = VulkanUtility::PathfinderPipelineStageToVulkan(signalPoints[i]->GetPipelineStages());
        signalSemaphoreInfos[i].deviceIndex = 0;
    }
    submitInfo2.pSignalSemaphoreInfos    = signalSemaphoreInfos.data();
    submitInfo2.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());

    const VkCommandBufferSubmitInfo commandBufferSI = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, nullptr, m_Handle, 0};
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
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;  // framebuffer and storage image case?
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
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
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;  // framebuffer and storage image case?
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
        default: PFR_ASSERT(false, "New layout is not supported!");
    }

    const auto vkSrcPipelineStages = VulkanUtility::PathfinderPipelineStageToVulkan(srcStageMask);
    const auto vkDstPipelineStages = VulkanUtility::PathfinderPipelineStageToVulkan(dstStageMask);

    const auto imageMemoryBarrier2 = VulkanUtility::GetImageMemoryBarrier(
        image, aspectMask, oldLayout, newLayout, vkSrcPipelineStages, VulkanUtility::PathfinderAccessFlagsToVulkan(srcAccessMask),
        vkDstPipelineStages, VulkanUtility::PathfinderAccessFlagsToVulkan(dstAccessMask), layerCount, baseLayer, mipLevels, baseMipLevel);

    InsertBarrier(vkSrcPipelineStages, vkDstPipelineStages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier2);
}

void VulkanCommandBuffer::InsertBarrier(const VkPipelineStageFlags2 srcStageMask, const VkPipelineStageFlags2 dstStageMask,
                                        const VkDependencyFlags dependencyFlags, const uint32_t memoryBarrierCount,
                                        const VkMemoryBarrier2* pMemoryBarriers, const uint32_t bufferMemoryBarrierCount,
                                        const VkBufferMemoryBarrier2* pBufferMemoryBarriers, const uint32_t imageMemoryBarrierCount,
                                        const VkImageMemoryBarrier2* pImageMemoryBarriers) const
{
    Renderer::GetStats().BarrierCount += memoryBarrierCount + bufferMemoryBarrierCount + imageMemoryBarrierCount;
    const VkDependencyInfo dependencyInfo = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                             nullptr,
                                             VK_DEPENDENCY_BY_REGION_BIT,
                                             memoryBarrierCount,
                                             pMemoryBarriers,
                                             bufferMemoryBarrierCount,
                                             pBufferMemoryBarriers,
                                             imageMemoryBarrierCount,
                                             pImageMemoryBarriers};
    vkCmdPipelineBarrier2(m_Handle, &dependencyInfo);
}

void VulkanCommandBuffer::BindShaderData(Shared<Pipeline>& pipeline, const Shared<Shader>& shader) const
{
    const auto vulkanPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    PFR_ASSERT(vulkanPipeline, "Failed to cast Pipeline to VulkanPipeline!");

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

    constexpr auto appendVecFunc = [&](auto& dst, const auto& src) { dst.insert(dst.end(), src.begin(), src.end()); };

    std::vector<VkDescriptorSet> descriptorSets;
    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            if (pipeline->GetPipelineOptions<GraphicsPipelineOptions>()->bMeshShading)
            {
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TASK));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_MESH));
            }
            else
            {
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
                appendVecFunc(descriptorSets,
                              vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));

                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
            }

            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_FRAGMENT));

            pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            break;
        }
        case EPipelineType::PIPELINE_TYPE_COMPUTE:
        {
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_COMPUTE));

            pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_ANY_HIT));
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_CLOSEST_HIT));
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_RAYGEN));
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_INTERSECTION));
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_CALLABLE));
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_MISS));

            pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
            break;
        }
    }

    if (descriptorSets.empty()) return;

    // Since first 0 is busy by bindless stuff we make an offset
    const uint32_t firstSet = pipeline->GetSpecification().bBindlessCompatible ? LAST_BINDLESS_SET + 1 : 0;
    vkCmdBindDescriptorSets(m_Handle, pipelineBindPoint, vulkanPipeline->GetLayout(), firstSet,
                            static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void VulkanCommandBuffer::BindPushConstants(Shared<Pipeline> pipeline, const uint32_t pushConstantIndex, const uint32_t offset,
                                            const uint32_t size, const void* data) const
{
    auto vulkanPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    PFR_ASSERT(vulkanPipeline, "Failed to cast Pipeline to VulkanPipeline!");

    const auto vkShaderStageFlags = vulkanPipeline->GetPushConstantShaderStageByIndex(pushConstantIndex);
    vkCmdPushConstants(m_Handle, vulkanPipeline->GetLayout(), vkShaderStageFlags, offset, size, data);
}

void VulkanCommandBuffer::Destroy()
{
    if (!m_Handle) return;

    auto& context = VulkanContext::Get();
    // context.GetDevice()->WaitDeviceOnFinish();
    context.GetDevice()->FreeCommandBuffer(m_Handle, m_Specification);

    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    vkDestroySemaphore(logicalDevice, m_TimelineSemaphore.Handle, VK_NULL_HANDLE);

    if (m_TimestampQuery) vkDestroyQueryPool(logicalDevice, m_TimestampQuery, nullptr);
    if (m_PipelineStatisticsQuery) vkDestroyQueryPool(logicalDevice, m_PipelineStatisticsQuery, nullptr);

    m_Handle = VK_NULL_HANDLE;
}

void VulkanCommandBuffer::BindDescriptorSets(Shared<VulkanPipeline>& pipeline, const uint32_t firstSet, const uint32_t descriptorSetCount,
                                             VkDescriptorSet* descriptorSets, const uint32_t dynamicOffsetCount,
                                             uint32_t* dynamicOffsets) const
{
    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
        case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
    }

    vkCmdBindDescriptorSets(m_Handle, pipelineBindPoint, pipeline->GetLayout(), firstSet, descriptorSetCount, descriptorSets,
                            dynamicOffsetCount, dynamicOffsets);
}

void VulkanCommandBuffer::BindPipeline(Shared<Pipeline>& pipeline) const
{
    // TODO: Remove dependency on main window, in case you wanna have more viewports
    const auto& window  = Application::Get().GetWindow();
    VkViewport viewport = {};
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;

    VkRect2D scissor = {{0, 0}, {window->GetSpecification().Width, window->GetSpecification().Height}};
    if (const auto* graphicsPipelineOptions = pipeline->GetPipelineOptions<GraphicsPipelineOptions>())
    {
        scissor.extent = VkExtent2D{graphicsPipelineOptions->TargetFramebuffer->GetSpecification().Width,
                                    graphicsPipelineOptions->TargetFramebuffer->GetSpecification().Height};

        viewport.y      = static_cast<float>(scissor.extent.height);
        viewport.width  = static_cast<float>(scissor.extent.width);
        viewport.height = -static_cast<float>(scissor.extent.height);
    }
    else
    {
        viewport = {0.0f,
                    static_cast<float>(window->GetSpecification().Height),
                    static_cast<float>(window->GetSpecification().Width),
                    -static_cast<float>(window->GetSpecification().Height),
                    0.0f,
                    1.0f};
    }

    if (const auto* graphicsPipelineOptions = pipeline->GetPipelineOptions<GraphicsPipelineOptions>())
    {
        if (graphicsPipelineOptions->bDynamicPolygonMode)
            vkCmdSetPolygonModeEXT(m_Handle, VulkanUtility::PathfinderPolygonModeToVulkan(graphicsPipelineOptions->PolygonMode));
    }

    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
        case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
    }

    if (const auto* graphicsPipelineOptions = pipeline->GetPipelineOptions<GraphicsPipelineOptions>())
    {
        vkCmdSetViewport(m_Handle, 0, 1, &viewport);
        vkCmdSetScissor(m_Handle, 0, 1, &scissor);

        if (graphicsPipelineOptions->LineWidth != 1.f) vkCmdSetLineWidth(m_Handle, graphicsPipelineOptions->LineWidth);
    }

    vkCmdBindPipeline(m_Handle, pipelineBindPoint, (VkPipeline)pipeline->Get());
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

void VulkanCommandBuffer::CopyImageToImage(const Shared<Image> srcImage, Shared<Image> dstImage) const
{
    PFR_ASSERT(srcImage && dstImage, "Can't copy image to image if images are invalid!");

    const VkImage vkSrcImage = (VkImage)srcImage->Get();
    const VkImage vkDstImage = (VkImage)dstImage->Get();

    // Preserve previous layout.
    const VkImageLayout vkSrcLayout = ImageUtils::PathfinderImageLayoutToVulkan(srcImage->GetSpecification().Layout);
    const VkImageLayout vkDstLayout = ImageUtils::PathfinderImageLayoutToVulkan(dstImage->GetSpecification().Layout);

    const VkImageAspectFlags srcAspectMask =
        ImageUtils::IsDepthFormat(srcImage->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    const VkImageAspectFlags dstAspectMask =
        ImageUtils::IsDepthFormat(dstImage->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    TransitionImageLayout(vkSrcImage, vkSrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcAspectMask, srcImage->GetSpecification().Layers,
                          0, srcImage->GetSpecification().Mips, 0);
    TransitionImageLayout(vkDstImage, vkDstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstAspectMask, dstImage->GetSpecification().Layers,
                          0, dstImage->GetSpecification().Mips, 0);

    VkImageCopy copyRegion = {};
    copyRegion.extent      = {dstImage->GetSpecification().Width, dstImage->GetSpecification().Height, 1};

    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.mipLevel       = 0;
    copyRegion.srcSubresource.layerCount     = srcImage->GetSpecification().Layers;
    copyRegion.srcSubresource.aspectMask     = srcAspectMask;

    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.mipLevel       = 0;
    copyRegion.dstSubresource.layerCount     = dstImage->GetSpecification().Layers;
    copyRegion.dstSubresource.aspectMask     = dstAspectMask;

    vkCmdCopyImage(m_Handle, vkSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &copyRegion);

    // Transition from transfer optimal to previous layout.
    TransitionImageLayout(vkSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkSrcLayout, srcAspectMask, srcImage->GetSpecification().Layers,
                          0, srcImage->GetSpecification().Mips, 0);
    TransitionImageLayout(vkDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vkDstLayout, dstAspectMask, dstImage->GetSpecification().Layers,
                          0, dstImage->GetSpecification().Mips, 0);
}

void VulkanCommandBuffer::FillBuffer(const Shared<Buffer>& buffer, const uint32_t data) const
{
    const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    vkCmdFillBuffer(m_Handle, (VkBuffer)vulkanBuffer->Get(), vulkanBuffer->GetDescriptorInfo().offset,
                    vulkanBuffer->GetDescriptorInfo().range, data);
}

}  // namespace Pathfinder