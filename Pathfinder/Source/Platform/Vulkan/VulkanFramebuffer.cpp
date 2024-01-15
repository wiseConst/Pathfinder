#include "PathfinderPCH.h"
#include "VulkanFramebuffer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Core/Application.h"
#include "Core/Window.h"

namespace Pathfinder
{

static VkAttachmentLoadOp GauntletLoadOpToVulkan(ELoadOp loadOp)
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

static VkAttachmentStoreOp GauntletStoreOpToVulkan(EStoreOp storeOp)
{
    switch (storeOp)
    {
        case EStoreOp::DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case EStoreOp::STORE: return VK_ATTACHMENT_STORE_OP_STORE;
    }

    PFR_ASSERT(false, "Unknown load op!");
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& framebufferSpec) : m_Specification(framebufferSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

const Shared<Image> VulkanFramebuffer::GetDepthAttachment() const
{
    // In case we own attachments
    for (auto& framebufferAttachment : m_Attachments)
    {
        if (ImageUtils::IsDepthFormat(framebufferAttachment.Specification.Format)) return framebufferAttachment.Attachment;
    }

    for (auto& framebufferAttachment : m_Specification.ExistingAttachments)
    {
        if (ImageUtils::IsDepthFormat(framebufferAttachment.Specification.Format)) return framebufferAttachment.Attachment;
    }

    return nullptr;
}

void VulkanFramebuffer::BeginPass(const Shared<CommandBuffer>& commandBuffer)
{
    auto vulkanCommandBuffer = static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer");

    vulkanCommandBuffer->BeginDebugLabel(m_Specification.Name.data(), glm::vec4(1.0f));

    // In case we own them
    // TODO: REVISIT THIS SHIT

    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    for (uint32_t i = 0; i < m_AttachmentInfos.size(); ++i)
    {
        auto vulkanImage = static_pointer_cast<VulkanImage>(m_Attachments.size() > 0 ? m_Attachments[i].Attachment
                                                                                     : m_Specification.ExistingAttachments[i].Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Imaage to VulkanImage");

        VkImageMemoryBarrier imageBarrier            = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.image                           = (VkImage)vulkanImage->Get();
        imageBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.baseMipLevel   = 0;

        imageBarrier.subresourceRange.levelCount = vulkanImage->GetSpecification().Mips;
        imageBarrier.subresourceRange.layerCount = 1;  // vulkanImage->GetSpecification().Layers;

        imageBarrier.oldLayout = ImageUtils::PathfinderImageLayoutToVulkan(vulkanImage->GetSpecification().Layout);

        imageBarrier.srcAccessMask = 0;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
        {
            depthAttachmentInfo    = m_AttachmentInfos[i];
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            // vulkanImage->SetLayout(imageBarrier.newLayout);
            vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                               VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        }
        else
        {
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            // vulkanImage->SetLayout(imageBarrier.newLayout);

            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        }
    }

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    if (vulkanCommandBuffer->GetLevel() == ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY)
        renderingInfo.flags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

    renderingInfo.renderArea           = {0, 0, m_Specification.Width, m_Specification.Height};
    renderingInfo.layerCount           = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_AttachmentInfos.size()) - (depthAttachmentInfo.imageView ? 1 : 0);
    renderingInfo.pColorAttachments    = m_AttachmentInfos.data();
    if (depthAttachmentInfo.imageView) renderingInfo.pDepthAttachment = &depthAttachmentInfo;
    // renderingInfo.pStencilAttachment   = &m_DepthStencilAttachmentInfo;

    vulkanCommandBuffer->BeginRendering(&renderingInfo);
    /* const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
     PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

     VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
     renderingInfo.renderArea      = {{0, 0}, {m_Specification.Width, m_Specification.Height}};

     vulkanCommandBuffer->BeginRendering(&renderingInfo);*/
}

void VulkanFramebuffer::EndPass(const Shared<CommandBuffer>& commandBuffer)
{
    auto vulkanCommandBuffer = static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer");

    vulkanCommandBuffer->EndRendering();

    vulkanCommandBuffer->EndDebugLabel();

    for (uint32_t i = 0; i < m_AttachmentInfos.size(); ++i)
    {
        auto vulkanImage = static_pointer_cast<VulkanImage>(m_Attachments.size() > 0 ? m_Attachments[i].Attachment
                                                                                     : m_Specification.ExistingAttachments[i].Attachment);

        VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageBarrier.image                = (VkImage)vulkanImage->Get();
        imageBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;

        imageBarrier.oldLayout = ImageUtils::PathfinderImageLayoutToVulkan(vulkanImage->GetSpecification().Layout);
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //        vulkanImage->SetLayout(imageBarrier.newLayout);
        vulkanImage->m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageBarrier.subresourceRange.baseArrayLayer = 0;
        imageBarrier.subresourceRange.baseMipLevel   = 0;
        imageBarrier.subresourceRange.levelCount     = vulkanImage->GetSpecification().Mips;
        imageBarrier.subresourceRange.layerCount     = 1;  // vulkanImage->GetSpecification().Layers;
        imageBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        if (ImageUtils::IsDepthFormat(vulkanImage->GetSpecification().Format))
        {
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            imageBarrier.srcAccessMask               = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                                               1, &imageBarrier);
        }
        else
        {
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.srcAccessMask               = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                               VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        }
    }
    /*const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    vulkanCommandBuffer->EndRendering();*/
}

void VulkanFramebuffer::Invalidate()
{
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    context.GetDevice()->WaitDeviceOnFinish();

    if (!m_Specification.ExistingAttachments.empty())
        PFR_ASSERT(m_Specification.Attachments.empty(), "You got existing attachments and you want to create new?");

    if (!m_Specification.Attachments.empty())
        PFR_ASSERT(m_Specification.ExistingAttachments.empty(), "You want to create attachments and you specified existing?");

    // Don't destroy what you don't own
    if (m_Specification.ExistingAttachments.empty())
    {
        for (auto& fbAttachment : m_Attachments)
            fbAttachment.Attachment->Resize(m_Specification.Width, m_Specification.Height);
    }

    m_AttachmentInfos.clear();

    // In case we wanna be owners
    if (m_Attachments.empty() && !m_Specification.Attachments.empty())
    {
        FramebufferAttachment newfbAttachment = {};
        ImageSpecification imageSpec          = {};

        for (auto& fbAttchament : m_Specification.Attachments)
        {
            imageSpec.Format = fbAttchament.Format;
            // imageSpec.Filter          = fbAttchament.Filter;
            // imageSpec.Wrap            = fbAttchament.Wrap;
            // imageSpec.CreateTextureID = true;
            imageSpec.Height = m_Specification.Height;
            imageSpec.Width  = m_Specification.Width;

            const EImageUsage additionalImageUsage = ImageUtils::IsDepthFormat(imageSpec.Format)
                                                         ? EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                         : EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            imageSpec.UsageFlags                   = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | additionalImageUsage;
            if (fbAttchament.bCopyable) imageSpec.UsageFlags |= EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;

            newfbAttachment.Attachment    = Image::Create(imageSpec);
            newfbAttachment.Specification = fbAttchament;  // copy the whole framebuffer attachment specification

            m_Attachments.push_back(newfbAttachment);
        }
    }

    m_DepthStencilAttachmentInfo                  = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    // TODO: Revisit with stenciling.

    // In case we got existing
    for (auto& fbAttachment : m_Specification.ExistingAttachments)
    {
        auto vulkanImage = static_pointer_cast<VulkanImage>(fbAttachment.Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        if (ImageUtils::IsDepthFormat(fbAttachment.Specification.Format))
        {
            depthAttachmentInfo.imageView               = (VkImageView)vulkanImage->GetView();
            depthAttachmentInfo.storeOp                 = GauntletStoreOpToVulkan(m_Specification.StoreOp);
            depthAttachmentInfo.loadOp                  = GauntletLoadOpToVulkan(m_Specification.LoadOp);
            depthAttachmentInfo.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0 /*128*/};
        }
        else
        {
            auto& attachment            = m_AttachmentInfos.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            attachment.imageView        = (VkImageView)vulkanImage->GetView();
            attachment.storeOp          = GauntletStoreOpToVulkan(m_Specification.StoreOp);
            attachment.loadOp           = GauntletLoadOpToVulkan(m_Specification.LoadOp);
            attachment.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.clearValue.color = {fbAttachment.Specification.ClearColor.x, fbAttachment.Specification.ClearColor.y,
                                           fbAttachment.Specification.ClearColor.z, fbAttachment.Specification.ClearColor.w};
        }
    }

    // Otherwise check for these
    for (auto& fbAttachment : m_Attachments)
    {
        auto vulkanImage = static_pointer_cast<VulkanImage>(fbAttachment.Attachment);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        if (ImageUtils::IsDepthFormat(fbAttachment.Specification.Format))
        {
            depthAttachmentInfo.imageView               = (VkImageView)vulkanImage->GetView();
            depthAttachmentInfo.storeOp                 = GauntletStoreOpToVulkan(fbAttachment.Specification.StoreOp);
            depthAttachmentInfo.loadOp                  = GauntletLoadOpToVulkan(fbAttachment.Specification.LoadOp);
            depthAttachmentInfo.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0 /*128*/};
        }
        else
        {
            auto& attachment            = m_AttachmentInfos.emplace_back(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
            attachment.imageView        = (VkImageView)vulkanImage->GetView();
            attachment.storeOp          = GauntletStoreOpToVulkan(fbAttachment.Specification.StoreOp);
            attachment.loadOp           = GauntletLoadOpToVulkan(fbAttachment.Specification.LoadOp);
            attachment.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.clearValue.color = {fbAttachment.Specification.ClearColor.x, fbAttachment.Specification.ClearColor.y,
                                           fbAttachment.Specification.ClearColor.z, fbAttachment.Specification.ClearColor.w};
        }
    }

    // Easy to handle them if depth is the last attachment
    if (depthAttachmentInfo.imageView) m_AttachmentInfos.push_back(depthAttachmentInfo);
}

void VulkanFramebuffer::Destroy()
{
    auto& context = (VulkanContext&)VulkanContext::Get();
    context.GetDevice()->WaitDeviceOnFinish();

    // Don't destroy what you don't own
    if (m_Specification.ExistingAttachments.empty()) m_Attachments.clear();

    m_AttachmentInfos.clear();
}

}  // namespace Pathfinder