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

static VkCommandBufferLevel PathfinderCommandBufferLevelToVulkan(ECommandBufferLevel level)
{
    switch (level)
    {
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY: return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    }

    PFR_ASSERT(false, "Unknown command buffer level!");
    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

static VkShaderStageFlags PathfinderShaderStageFlagsToVulkan(const ShaderStageFlags shaderStageFlags)
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

VulkanCommandBuffer::VulkanCommandBuffer(ECommandBufferType type, bool bSignaledFence, ECommandBufferLevel level)
    : m_Type(type), m_Level(level)
{
    const auto& context = VulkanContext::Get();
    context.GetDevice()->AllocateCommandBuffer(m_Handle, type, PathfinderCommandBufferLevelToVulkan(m_Level));

    std::string commandBufferTypeStr;
    switch (m_Type)
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

    {
        VkSemaphoreCreateInfo semaphoreCI = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(context.GetDevice()->GetLogicalDevice(), &semaphoreCI, VK_NULL_HANDLE, &m_SignalSemaphore),
                 "Failed to create binary semaphore!");

        const std::string semaphoreDebugName = "VK_SIGNAL_SEMAPHORE_" + commandBufferTypeStr;
        VK_SetDebugName(context.GetDevice()->GetLogicalDevice(), m_SignalSemaphore, VK_OBJECT_TYPE_SEMAPHORE, semaphoreDebugName.data());
    }

    {
        const VkFenceCreateInfo fenceCI = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                                           bSignaledFence ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{}};
        VK_CHECK(vkCreateFence(context.GetDevice()->GetLogicalDevice(), &fenceCI, VK_NULL_HANDLE, &m_SubmitFence),
                 "Failed to create fence!");

        const std::string fenceDebugName = "VK_SUBMIT_FENCE_" + commandBufferTypeStr;
        VK_SetDebugName(context.GetDevice()->GetLogicalDevice(), m_SubmitFence, VK_OBJECT_TYPE_FENCE, fenceDebugName.data());
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
    const float timestampPeriod = VulkanContext::Get().GetDevice()->GetGPUProperties().limits.timestampPeriod;

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
        if (m_Type == ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS)
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
                VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

            if (Renderer::GetRendererSettings().bMeshShadingSupport)
                queryPoolCI.pipelineStatistics |= VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT |
                                                  VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT;
        }
        else if (m_Type == ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE)
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

void VulkanCommandBuffer::InsertBufferMemoryBarrier(const Shared<Buffer> buffer, const EPipelineStage srcPipelineStage,
                                                    const EPipelineStage dstPipelineStage, const EAccessFlags srcAccessFlags,
                                                    const EAccessFlags dstAccessFlags) const
{
    const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    const auto vkSrcAccessFlags = VulkanUtility::PathfinderAccessFlagsToVulkan(srcAccessFlags);
    const auto vkDstAccessFlags = VulkanUtility::PathfinderAccessFlagsToVulkan(dstAccessFlags);

    const VkBuffer rawBuffer       = (VkBuffer)vulkanBuffer->Get();
    const auto bufferMemoryBarrier = VulkanUtility::GetBufferMemoryBarrier(rawBuffer, vulkanBuffer->GetSpecification().BufferCapacity, 0,
                                                                           vkSrcAccessFlags, vkDstAccessFlags);
    InsertBarrier(VulkanUtility::PathfinderPipelineStageToVulkan(srcPipelineStage),
                  VulkanUtility::PathfinderPipelineStageToVulkan(dstPipelineStage), VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1,
                  &bufferMemoryBarrier, 0, nullptr);
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

    if (m_Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        PFR_ASSERT(inheritanceInfo, "Secondary command buffer requires valid inheritance info!");
    }

    if (bOneTimeSubmit) commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    commandBufferBeginInfo.pInheritanceInfo = static_cast<const VkCommandBufferInheritanceInfo*>(inheritanceInfo);
    VK_CHECK(vkBeginCommandBuffer(m_Handle, &commandBufferBeginInfo), "Failed to begin command buffer recording!");

    if (m_Type == ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER) return;

    m_CurrentTimestampIndex = 0;
}

void VulkanCommandBuffer::Submit(bool bWaitAfterSubmit, bool bSignalWaitSemaphore, uint64_t timelineSignalValue,
                                 const std::vector<PipelineStageFlags> pipelineStages, const std::vector<void*>& waitSemaphores,
                                 const std::vector<uint64_t>& waitSemaphoreValues, const std::vector<void*>& signalSemaphores,
                                 const std::vector<uint64_t>& signalSemaphoreValues)
{
    VkSubmitInfo submitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_Handle;

    VkTimelineSemaphoreSubmitInfo timelineSemaphoreSI = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    submitInfo.pNext                                  = &timelineSemaphoreSI;

    // Managing all signal semaphores.
    std::vector<VkSemaphore> finalSignalSemaphores = {};
    if (bSignalWaitSemaphore) finalSignalSemaphores.emplace_back(m_SignalSemaphore);
    if (timelineSignalValue != UINT64_MAX) finalSignalSemaphores.emplace_back(m_TimelineSemaphore.Handle);
    for (const auto& signalSem : signalSemaphores)
        finalSignalSemaphores.emplace_back((VkSemaphore)signalSem);

    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(finalSignalSemaphores.size());
    submitInfo.pSignalSemaphores    = finalSignalSemaphores.data();

    // Managing signal's semaphores values.
    std::vector<uint64_t> finalSignalValues = {};
    if (bSignalWaitSemaphore) finalSignalValues.emplace_back(1);
    if (timelineSignalValue != UINT64_MAX) finalSignalValues.emplace_back(timelineSignalValue);

    finalSignalValues.insert(finalSignalValues.end(), signalSemaphoreValues.begin(), signalSemaphoreValues.end());
    timelineSemaphoreSI.signalSemaphoreValueCount = static_cast<uint32_t>(finalSignalValues.size());
    timelineSemaphoreSI.pSignalSemaphoreValues    = finalSignalValues.data();

    // Managing wait part.
    timelineSemaphoreSI.waitSemaphoreValueCount = waitSemaphoreValues.size();
    timelineSemaphoreSI.pWaitSemaphoreValues    = waitSemaphoreValues.data();

    std::vector<VkPipelineStageFlags> waitDstStageMask;
    for (auto& pipelineStageFlags : pipelineStages)
        waitDstStageMask.emplace_back(VulkanUtility::PathfinderPipelineStageToVulkan(pipelineStageFlags));

    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores    = (const VkSemaphore*)waitSemaphores.data();
    submitInfo.pWaitDstStageMask  = waitDstStageMask.data();

    auto& context = VulkanContext::Get();
    VkQueue queue = VK_NULL_HANDLE;
    switch (m_Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS:
        {
            queue = context.GetDevice()->GetGraphicsQueue();
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE:
        {
            queue = context.GetDevice()->GetComputeQueue();
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER:
        {
            queue = context.GetDevice()->GetTransferQueue();
            break;
        }
    }

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, timelineSignalValue == UINT64_MAX ? m_SubmitFence : VK_NULL_HANDLE),
             "Failed to submit command buffer!");
    if (bWaitAfterSubmit)
    {
        const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

        VK_CHECK(vkWaitForFences(logicalDevice, 1, &m_SubmitFence, VK_TRUE, UINT64_MAX), "Failed to wait for fence!");
        VK_CHECK(vkResetFences(logicalDevice, 1, &m_SubmitFence), "Failed to reset fence!");
    }
}

void VulkanCommandBuffer::TransitionImageLayout(const VkImage& image, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                                const VkImageAspectFlags aspectMask, const uint32_t layerCount,
                                                const uint32_t mipLevels) const
{
    if (oldLayout == newLayout) return;

    VkAccessFlags srcAccessMask             = 0;
    VkAccessFlags dstAccessMask             = 0;
    VkImageMemoryBarrier imageMemoryBarrier = VulkanUtility::GetImageMemoryBarrier(image, aspectMask, oldLayout, newLayout, srcAccessMask,
                                                                                   dstAccessMask, layerCount, 0, mipLevels, 0);

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    // Determine stages and resource access:
    switch (oldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
        {
            srcAccessMask = 0;
            srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_GENERAL:  // NOTE: May be not optimal stage
        {
            srcAccessMask = 0;
            srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask =
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;  // framebuffer and storage image case?
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        {
            srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }
        default: PFR_ASSERT(false, "Old layout is not supported!");
    }

    switch (newLayout)
    {
        case VK_IMAGE_LAYOUT_GENERAL:
        {
            dstAccessMask = 0;  // GENERAL layout means resource can be affected by any operations
            dstStageMask  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStageMask =
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;  // framebuffer and storage image case?
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        {
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }
        default: PFR_ASSERT(false, "New layout is not supported!");
    }

    imageMemoryBarrier.srcAccessMask = srcAccessMask;
    imageMemoryBarrier.dstAccessMask = dstAccessMask;
    InsertBarrier(srcStageMask, dstStageMask, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void VulkanCommandBuffer::BindShaderData(Shared<Pipeline>& pipeline, const Shared<Shader>& shader) const
{
    const auto vulkanPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    PFR_ASSERT(vulkanPipeline, "Failed to cast Pipeline to VulkanPipeline!");

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

    const auto appendVecFunc = [&](auto& src, const auto& additionalVec)
    {
        for (auto& elem : additionalVec)
            src.emplace_back(elem);
    };

    std::vector<VkDescriptorSet> descriptorSets;
    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_FRAGMENT));

            if (pipeline->GetSpecification().bMeshShading)
            {
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_MESH));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TASK));
            }
            else
            {
                // clang-format off
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
                appendVecFunc(descriptorSets, vulkanShader->GetDescriptorSetByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));
                // clang-format on
            }

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

    // Since first 0, 1, 2 are busy by bindless stuff we make an offset
    const uint32_t firstSet = pipeline->GetSpecification().bBindlessCompatible ? LAST_BINDLESS_SET + 1 : 0;
    vkCmdBindDescriptorSets(m_Handle, pipelineBindPoint, vulkanPipeline->GetLayout(), firstSet,
                            static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void VulkanCommandBuffer::BindPushConstants(Shared<Pipeline>& pipeline, const uint32_t pushConstantIndex, const uint32_t offset,
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
    context.GetDevice()->WaitDeviceOnFinish();
    context.GetDevice()->FreeCommandBuffer(m_Handle, m_Type);

    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    vkDestroyFence(logicalDevice, m_SubmitFence, VK_NULL_HANDLE);
    vkDestroySemaphore(logicalDevice, m_SignalSemaphore, VK_NULL_HANDLE);
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
    if (const auto& targetFramebuffer = pipeline->GetSpecification().TargetFramebuffer)
    {
        scissor.extent = VkExtent2D{targetFramebuffer->GetSpecification().Width, targetFramebuffer->GetSpecification().Height};

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

    if (pipeline->GetSpecification().bDynamicPolygonMode)
        vkCmdSetPolygonModeEXT(m_Handle, VulkanUtility::PathfinderPolygonModeToVulkan(pipeline->GetSpecification().PolygonMode));

    VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    switch (pipeline->GetSpecification().PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
        case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
    }

    if (pipelineBindPoint != VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        vkCmdSetViewport(m_Handle, 0, 1, &viewport);
        vkCmdSetScissor(m_Handle, 0, 1, &scissor);
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
                          srcImage->GetSpecification().Mips);
    TransitionImageLayout(vkDstImage, vkDstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstAspectMask, dstImage->GetSpecification().Layers,
                          dstImage->GetSpecification().Mips);

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
                          srcImage->GetSpecification().Mips);
    TransitionImageLayout(vkDstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vkDstLayout, dstAspectMask, dstImage->GetSpecification().Layers,
                          dstImage->GetSpecification().Mips);
}

}  // namespace Pathfinder