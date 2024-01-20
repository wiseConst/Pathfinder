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

VulkanCommandBuffer::VulkanCommandBuffer(ECommandBufferType type, ECommandBufferLevel level) : m_Type(type), m_Level(level)
{
    auto& context = VulkanContext::Get();
    context.GetDevice()->AllocateCommandBuffer(m_Handle, type, PathfinderCommandBufferLevelToVulkan(m_Level));

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
        const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
        VK_CHECK(vkWaitForFences(logicalDevice, 1, &m_SubmitFence, VK_TRUE, UINT64_MAX), "Failed to wait for fence!");
        VK_CHECK(vkResetFences(logicalDevice, 1, &m_SubmitFence), "Failed to reset fence!");
    }
}

// NOTE: Mip mapping and layer count harcoded for now
void VulkanCommandBuffer::TransitionImageLayout(const VkImage& image, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                                const VkImageAspectFlags aspectMask)
{
    if (oldLayout == newLayout) return;

    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;
    const VkImageMemoryBarrier imageMemoryBarrier =
        VulkanUtility::GetImageMemoryBarrier(image, aspectMask, oldLayout, newLayout, srcAccessMask, dstAccessMask, 1, 0, 1, 0);

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;

    // Determine stages and resource access:
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        srcAccessMask = 0;
        dstAccessMask = 0;  // GENERAL layout means resource can be affected by any operationr

        srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;  // framebuffer and storage image case?
    }
    else
    {
        PFR_ASSERT(false, "Other transitions not supported!");
    }

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

    // Since first 0,1,2 are busy by bindless stuff we make an offset
    const uint32_t firstSet = pipeline->GetSpecification().bBindlessCompatible ? 4 : 0;
    vkCmdBindDescriptorSets(m_Handle, pipelineBindPoint, vulkanPipeline->GetLayout(), firstSet,
                            static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void VulkanCommandBuffer::BindPushConstants(Shared<Pipeline>& pipeline, const uint32_t pushConstantIndex, const uint32_t offset,
                                            const uint32_t size, const void* data) const
{
    auto vulkanPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    PFR_ASSERT(vulkanPipeline, "Failed to cast Pipeline to VulkanPipeline!");

    const VkShaderStageFlags vkShaderStageFlags = vulkanPipeline->GetPushConstantShaderStageByIndex(pushConstantIndex);
    vkCmdPushConstants(m_Handle, vulkanPipeline->GetLayout(), vkShaderStageFlags, offset, size, data);
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
    VkViewport viewport = {0.0f,
                           static_cast<float>(window->GetSpecification().Height),
                           static_cast<float>(window->GetSpecification().Width),
                           -static_cast<float>(window->GetSpecification().Height),
                           0.0f,
                           1.0f};

    VkRect2D scissor = {{0, 0}, {window->GetSpecification().Width, window->GetSpecification().Height}};
    if (auto& targetFramebuffer = pipeline->GetSpecification().TargetFramebuffer[window->GetCurrentFrameIndex()])
    {
        scissor.extent = VkExtent2D{targetFramebuffer->GetSpecification().Width, targetFramebuffer->GetSpecification().Height};

        viewport.y      = static_cast<float>(scissor.extent.height);
        viewport.width  = static_cast<float>(scissor.extent.width);
        viewport.height = -static_cast<float>(scissor.extent.height);
    }

    // if (pipeline->GetSpecification().bDynamicPolygonMode && !RENDERDOC_DEBUG)
    //       vkCmdSetPolygonModeEXT(m_Handle, Utility::GauntletPolygonModeToVulkan(pipeline->GetSpecification().PolygonMode));

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

}  // namespace Pathfinder