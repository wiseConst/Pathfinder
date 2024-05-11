#include "PathfinderPCH.h"
#include "VulkanFramebuffer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "Renderer/Renderer.h"
#include "Renderer/BindlessRenderer.h"

namespace Pathfinder
{

static VkAttachmentLoadOp PathfinderLoadOpToVulkan(ELoadOp loadOp)
{
    switch (loadOp)
    {
        case ELoadOp::DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case ELoadOp::CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case ELoadOp::LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    PFR_ASSERT(false, "Unknown load op!");
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp PathfinderStoreOpToVulkan(EStoreOp storeOp)
{
    switch (storeOp)
    {
        case EStoreOp::DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case EStoreOp::STORE: return VK_ATTACHMENT_STORE_OP_STORE;
    }

    PFR_ASSERT(false, "Unknown store op!");
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& framebufferSpec) : Framebuffer(framebufferSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

Shared<Image> VulkanFramebuffer::GetDepthAttachment() const
{
    for (const auto& framebufferAttachment : m_Attachments)
    {
        if (ImageUtils::IsDepthFormat(framebufferAttachment.Specification.Format)) return framebufferAttachment.Attachment;
    }

    return nullptr;
}

void VulkanFramebuffer::BeginPass(const Shared<CommandBuffer>& commandBuffer)
{
    auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer");

    // To batch pipeline barriers submits
    std::vector<VkImageMemoryBarrier2> colorImageBarriers;
    VkImageMemoryBarrier2 depthImageBarrier = {};

    uint32_t layerCount                           = 0;
    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    for (size_t i = 0; i < m_AttachmentInfos.size(); ++i)
    {
        const auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Attachments[i].Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage");

        VkImageMemoryBarrier2 imageBarrier           = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image                           = (VkImage)vulkanImage->Get();
        imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;  // No resource migration for the queue
        imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.baseMipLevel   = 0;

        imageBarrier.subresourceRange.levelCount = vulkanImage->GetSpecification().Mips;
        imageBarrier.subresourceRange.layerCount = vulkanImage->GetSpecification().Layers;
        layerCount                               = std::max(layerCount, imageBarrier.subresourceRange.layerCount);

        VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
        if (ImageUtils::IsStencilFormat(vulkanImage->GetSpecification().Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
            imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

        imageBarrier.oldLayout     = ImageUtils::PathfinderImageLayoutToVulkan(vulkanImage->GetSpecification().Layout);
        imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
        {
            PFR_ASSERT(i + 1 == m_AttachmentInfos.size(), "I assume that the last one is depth attachment!");
            PFR_ASSERT(!depthImageBarrier.image, "Depth attachment already setup!");

            depthAttachmentInfo                 = m_AttachmentInfos[i];
            imageBarrier.newLayout              = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            imageBarrier.subresourceRange.aspectMask = imageAspectMask;
            imageBarrier.dstAccessMask               = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            // NOTE: In case we previously cleared/upscaled/rendered somewhere current attachment, we should assure this op's completed.
            if (m_AttachmentInfos[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                imageBarrier.dstAccessMask |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                imageBarrier.srcStageMask |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            }

            depthImageBarrier = imageBarrier;
        }
        else
        {
            imageBarrier.newLayout              = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            imageBarrier.subresourceRange.aspectMask = imageAspectMask;
            imageBarrier.dstAccessMask               = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

            // NOTE: In case we previously cleared/upscaled/rendered somewhere current attachment, we should assure this op's completed.
            if (m_AttachmentInfos[i].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                imageBarrier.dstAccessMask |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
                imageBarrier.srcStageMask |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            }

            colorImageBarriers.emplace_back(imageBarrier);
        }
    }

    if (depthImageBarrier.image)
    {
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &depthImageBarrier);
    }

    if (!colorImageBarriers.empty())
    {
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                                           static_cast<uint32_t>(colorImageBarriers.size()), colorImageBarriers.data());
    }

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    if (vulkanCommandBuffer->GetSpecification().Level == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
        renderingInfo.flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    renderingInfo.renderArea = {0, 0, m_Specification.Width, m_Specification.Height};
    renderingInfo.layerCount = layerCount;

    if (const auto colorAttachmentCount = static_cast<uint32_t>(m_AttachmentInfos.size()) - (depthAttachmentInfo.imageView ? 1 : 0);
        colorAttachmentCount > 0)
    {
        renderingInfo.colorAttachmentCount = colorAttachmentCount;
        renderingInfo.pColorAttachments    = m_AttachmentInfos.data();
    }
    if (depthAttachmentInfo.imageView) renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    vulkanCommandBuffer->BeginDebugLabel(m_Specification.Name.data(), glm::vec3(0.0f));
    vulkanCommandBuffer->BeginRendering(&renderingInfo);
}

void VulkanFramebuffer::EndPass(const Shared<CommandBuffer>& commandBuffer)
{
    auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer");

    vulkanCommandBuffer->EndRendering();
    vulkanCommandBuffer->EndDebugLabel();

    std::vector<VkImageMemoryBarrier2> colorImageBarriers;
    VkImageMemoryBarrier2 depthImageBarrier = {};
    for (size_t i = 0; i < m_AttachmentInfos.size(); ++i)
    {
        auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Attachments[i].Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage");

        VkImageMemoryBarrier2 imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image                 = (VkImage)vulkanImage->Get();
        imageBarrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;

        imageBarrier.oldLayout              = ImageUtils::PathfinderImageLayoutToVulkan(vulkanImage->GetSpecification().Layout);
        imageBarrier.newLayout              = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
        if (ImageUtils::IsStencilFormat(vulkanImage->GetSpecification().Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
            imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.baseMipLevel   = 0;
        imageBarrier.subresourceRange.levelCount     = vulkanImage->GetSpecification().Mips;
        imageBarrier.subresourceRange.layerCount     = vulkanImage->GetSpecification().Layers;
        imageBarrier.dstAccessMask                   = VK_ACCESS_2_SHADER_READ_BIT;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
        {
            PFR_ASSERT(i + 1 == m_AttachmentInfos.size(), "I assume that the last one is depth attachment!");
            PFR_ASSERT(!depthImageBarrier.image, "Depth attachment already setup!");

            imageBarrier.subresourceRange.aspectMask = imageAspectMask;
            imageBarrier.srcAccessMask               = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

            depthImageBarrier = imageBarrier;
        }
        else
        {
            imageBarrier.subresourceRange.aspectMask = imageAspectMask;
            imageBarrier.srcAccessMask               = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

            imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

            colorImageBarriers.emplace_back(imageBarrier);
        }
    }

    if (depthImageBarrier.image)
    {
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1,
                                           &depthImageBarrier);
    }

    if (!colorImageBarriers.empty())
    {
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                                           static_cast<uint32_t>(colorImageBarriers.size()), colorImageBarriers.data());
    }
}

void VulkanFramebuffer::Invalidate()
{
    // Clear previous info needed for rendering into attachments
    m_AttachmentInfos.clear();

    PFR_ASSERT(!m_Specification.Attachments.empty(), "Info about attachments is not specified!");
    for (size_t attachmentIndex = 0; attachmentIndex < m_Attachments.size(); ++attachmentIndex)
    {
        if (m_Specification.ExistingAttachments.contains(attachmentIndex)) continue;

        auto& fbAttachment = m_Attachments[attachmentIndex];
        if (fbAttachment.Attachment->GetSpecification().Width != m_Specification.Width ||
            fbAttachment.Attachment->GetSpecification().Height != m_Specification.Height)
            fbAttachment.Attachment->Resize(m_Specification.Width, m_Specification.Height);

#if PFR_DEBUG
        const std::string framebufferAttachmentStr = m_Specification.Name + "_Attachment_" + std::to_string(attachmentIndex);
        VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Attachments[attachmentIndex].Attachment->Get(),
                        VK_OBJECT_TYPE_IMAGE, framebufferAttachmentStr.data());
#endif

        if (fbAttachment.Specification.bBindlessUsage && fbAttachment.m_Index != UINT32_MAX)
        {
            const auto vulkanImage = std::static_pointer_cast<VulkanImage>(fbAttachment.Attachment);
            PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

            Renderer::GetBindlessRenderer()->FreeTexture(fbAttachment.m_Index);
            Renderer::GetBindlessRenderer()->LoadTexture(&vulkanImage->GetDescriptorInfo(), fbAttachment.m_Index);
        }
    }

    // NOTE: On first Invalidate()-call we create attachments
    if (m_Attachments.empty())
    {
        for (auto& pair : m_Specification.ExistingAttachments)
            PFR_ASSERT(pair.first < m_Specification.Attachments.size(), "Unreachable existing attachment!");

        m_Attachments.resize(m_Specification.Attachments.size());
        for (size_t attachmentIndex = 0; attachmentIndex < m_Specification.Attachments.size(); ++attachmentIndex)
        {
            // Firstly copy attachment specification
            const auto& fbAttachmentSpec                 = m_Specification.Attachments[attachmentIndex];
            m_Attachments[attachmentIndex].Specification = fbAttachmentSpec;

            // Handle existing attachment, otherwise create new one
            if (m_Specification.ExistingAttachments.contains(attachmentIndex))
            {
                PFR_ASSERT(fbAttachmentSpec.Format == m_Specification.ExistingAttachments[attachmentIndex]->GetSpecification().Format,
                           "Attachment formats don't match!");
                m_Attachments[attachmentIndex].Attachment = m_Specification.ExistingAttachments[attachmentIndex];
            }
            else
            {
                ImageSpecification imageSpec = {m_Specification.Width, m_Specification.Height, fbAttachmentSpec.Format};
                imageSpec.Layout             = fbAttachmentSpec.Layout;
                imageSpec.Wrap               = fbAttachmentSpec.Wrap;
                imageSpec.Filter             = fbAttachmentSpec.Filter;
                imageSpec.Layers             = fbAttachmentSpec.LayerCount;

                const auto imageUsage = ImageUtils::IsDepthFormat(imageSpec.Format) ? EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                                                    : EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                imageSpec.UsageFlags  = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | imageUsage;
                if (fbAttachmentSpec.bCopyable)
                    imageSpec.UsageFlags |= EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;

                m_Attachments[attachmentIndex].Attachment = Image::Create(imageSpec);

#if PFR_DEBUG
                const std::string framebufferAttachmentStr = m_Specification.Name + "_Attachment_" + std::to_string(attachmentIndex);
                VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Attachments[attachmentIndex].Attachment->Get(),
                                VK_OBJECT_TYPE_IMAGE, framebufferAttachmentStr.data());
#endif

                if (fbAttachmentSpec.bBindlessUsage && m_Attachments[attachmentIndex].m_Index == UINT32_MAX)
                {
                    const auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Attachments[attachmentIndex].Attachment);
                    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

                    Renderer::GetBindlessRenderer()->LoadTexture(&vulkanImage->GetDescriptorInfo(), m_Attachments[attachmentIndex].m_Index);
                }
            }
        }
    }

    // Cache info needed for rendering attachments
    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    m_AttachmentInfos.reserve(m_Attachments.size());

    for (size_t attachmentIndex = 0; attachmentIndex < m_Attachments.size(); ++attachmentIndex)
    {
        const auto& fbAttachment = m_Attachments[attachmentIndex];
        const auto vulkanImage   = std::static_pointer_cast<VulkanImage>(fbAttachment.Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        if (ImageUtils::IsDepthFormat(fbAttachment.Specification.Format))
        {
            depthAttachmentInfo.imageView               = vulkanImage->GetView();
            depthAttachmentInfo.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.storeOp                 = PathfinderStoreOpToVulkan(fbAttachment.Specification.StoreOp);
            depthAttachmentInfo.loadOp                  = PathfinderLoadOpToVulkan(fbAttachment.Specification.LoadOp);
            depthAttachmentInfo.clearValue.depthStencil = {fbAttachment.Specification.ClearColor.x,
                                                           static_cast<uint32_t>(fbAttachment.Specification.ClearColor.y)};
        }
        else
        {
            auto& attachment            = m_AttachmentInfos.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            attachment.imageView        = vulkanImage->GetView();
            attachment.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.storeOp          = PathfinderStoreOpToVulkan(fbAttachment.Specification.StoreOp);
            attachment.loadOp           = PathfinderLoadOpToVulkan(fbAttachment.Specification.LoadOp);
            attachment.clearValue.color = {fbAttachment.Specification.ClearColor.x, fbAttachment.Specification.ClearColor.y,
                                           fbAttachment.Specification.ClearColor.z, fbAttachment.Specification.ClearColor.w};
        }
    }

    // Easy to handle them if depth is the last attachment
    if (depthAttachmentInfo.imageView) m_AttachmentInfos.emplace_back(depthAttachmentInfo);
}

void VulkanFramebuffer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    for (auto& fbAttachment : m_Attachments)
    {
        if (fbAttachment.Specification.bBindlessUsage && fbAttachment.m_Index != UINT32_MAX)
        {
            Renderer::GetBindlessRenderer()->FreeTexture(fbAttachment.m_Index);
        }
    }

    m_Attachments.clear();
    m_AttachmentInfos.clear();
}

void VulkanFramebuffer::Clear(const Shared<CommandBuffer>& commandBuffer)
{
    for (const auto& attachmentInfo : m_Attachments)
    {
        if (!attachmentInfo.Specification.bCopyable) continue;

        attachmentInfo.Attachment->ClearColor(commandBuffer, attachmentInfo.Specification.ClearColor);
    }
}

}  // namespace Pathfinder