#include "PathfinderPCH.h"
#include "VulkanCommandBuffer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"

namespace Pathfinder
{

static VkCommandBufferLevel GauntletCommandBufferLevelToVulkan(ECommandBufferLevel level)
{
    switch (level)
    {
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY: return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    }

    PFR_ASSERT(false, "Unknown command buffer level!");
    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

VulkanCommandBuffer::VulkanCommandBuffer(ECommandBufferType type, ECommandBufferLevel level) : m_Type(type), m_Level(level)
{
    auto& context = VulkanContext::Get();
    context.GetDevice()->AllocateCommandBuffer(m_Handle, type, GauntletCommandBufferLevelToVulkan(m_Level));

    CreateSyncResourcesAndQueries();
}

void VulkanCommandBuffer::CreateSyncResourcesAndQueries()
{
    auto& context                               = VulkanContext::Get();
    constexpr VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(context.GetDevice()->GetLogicalDevice(), &fenceCreateInfo, VK_NULL_HANDLE, &m_SubmitFence),
             "Failed to create fence!");
}

void VulkanCommandBuffer::BeginRecording(bool bOneTimeSubmit, const void* inheritanceInfo)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    if (m_Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
    {
        commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        PFR_ASSERT(inheritanceInfo, "Secondary command buffer requires valid inheritance info!");
    }

    if (bOneTimeSubmit) commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    commandBufferBeginInfo.pInheritanceInfo = static_cast<const VkCommandBufferInheritanceInfo*>(inheritanceInfo);
    VK_CHECK(vkBeginCommandBuffer(m_Handle, &commandBufferBeginInfo), "Failed to begin command buffer recording!");
}

void VulkanCommandBuffer::Submit(bool bWaitAfterSubmit)
{
    VkSubmitInfo submitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_Handle;

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

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, m_SubmitFence), "Failed to submit command buffer!");

    if (bWaitAfterSubmit)
    {
        VK_CHECK(vkWaitForFences(context.GetDevice()->GetLogicalDevice(), 1, &m_SubmitFence, VK_TRUE, UINT64_MAX),
                 "Failed to wait for fence!");
        VK_CHECK(vkResetFences(context.GetDevice()->GetLogicalDevice(), 1, &m_SubmitFence), "Failed to reset fence!");
    }
}

void VulkanCommandBuffer::TransitionImageLayout(const Shared<Image>& image, const EImageLayout newLayout)
{



}

void VulkanCommandBuffer::Destroy()
{
    if (!m_Handle) return;

    auto& context = VulkanContext::Get();
    context.GetDevice()->WaitDeviceOnFinish();

    context.GetDevice()->FreeCommandBuffer(m_Handle, m_Type);
    vkDestroyFence(context.GetDevice()->GetLogicalDevice(), m_SubmitFence, VK_NULL_HANDLE);

    m_Handle = VK_NULL_HANDLE;
}

void VulkanCommandBuffer::BindDescriptorSets(Shared<VulkanPipeline>& pipeline, const uint32_t firstSet, const uint32_t descriptorSetCount,
                                             VkDescriptorSet* descriptorSets, const uint32_t dynamicOffsetCount,
                                             uint32_t* dynamicOffsets) const
{
    // VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    // switch (pipeline->GetSpecification().PipelineType)
    // {
    //     case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
    //     case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
    //     case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
    // }
    //
    // vkCmdBindDescriptorSets(m_Handle, pipelineBindPoint, pipeline->GetLayout(), firstSet, descriptorSetCount, descriptorSets,
    //                         dynamicOffsetCount, dynamicOffsets);
}

// void VulkanCommandBuffer::BindPipeline(Shared<VulkanPipeline>& pipeline) const
// {
//     auto& context         = VulkanContext::Get();
//     const auto& swapchain = context.GetSwapchain();
//     PFR_ASSERT(swapchain->IsValid(), "Vulkan swapchain is not valid!");
//
//     VkViewport viewport = {};
//     viewport.x          = 0.0f;
//     viewport.minDepth   = 0.0f;
//     viewport.maxDepth   = 1.0f;
//
//     viewport.y      = static_cast<float>(swapchain->GetImageExtent().height);
//     viewport.width  = static_cast<float>(swapchain->GetImageExtent().width);
//     viewport.height = -static_cast<float>(swapchain->GetImageExtent().height);
//
//     VkRect2D scissor = {{0, 0}, swapchain->GetImageExtent()};
//     if (auto& targetFramebuffer = pipeline->GetSpecification().TargetFramebuffer[context.GetCurrentFrameIndex()])
//     {
//         scissor.extent = VkExtent2D{targetFramebuffer->GetWidth(), targetFramebuffer->GetHeight()};
//
//         viewport.y      = static_cast<float>(scissor.extent.height);
//         viewport.width  = static_cast<float>(scissor.extent.width);
//         viewport.height = -static_cast<float>(scissor.extent.height);
//     }
//
//     if (pipeline->GetSpecification().bDynamicPolygonMode && !RENDERDOC_DEBUG)
//         vkCmdSetPolygonModeEXT(m_Handle, Utility::GauntletPolygonModeToVulkan(pipeline->GetSpecification().PolygonMode));
//
//     VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//     switch (pipeline->GetSpecification().PipelineType)
//     {
//         case EPipelineType::PIPELINE_TYPE_COMPUTE: pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE; break;
//         case EPipelineType::PIPELINE_TYPE_GRAPHICS: pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; break;
//         case EPipelineType::PIPELINE_TYPE_RAY_TRACING: pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; break;
//     }
//
//     vkCmdBindPipeline(m_Handle, pipelineBindPoint, pipeline->Get());
//
//     if (pipelineBindPoint != VK_PIPELINE_BIND_POINT_COMPUTE)
//     {
//         vkCmdSetViewport(m_Handle, 0, 1, &viewport);
//         vkCmdSetScissor(m_Handle, 0, 1, &scissor);
//     }
// }

// void VulkanCommandBuffer::SetPipelinePolygonMode(Ref<VulkanPipeline>& pipeline, const EPolygonMode polygonMode) const
// {
//     pipeline->GetSpecification().PolygonMode = polygonMode;
// }

}  // namespace Pathfinder