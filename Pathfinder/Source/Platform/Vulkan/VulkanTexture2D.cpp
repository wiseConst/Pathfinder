#include <PathfinderPCH.h>
#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include "VulkanCommandBuffer.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

VulkanTexture2D::VulkanTexture2D(const TextureSpecification& textureSpec, const void* data, const size_t dataSize) : Texture2D(textureSpec)
{
    Invalidate(data, dataSize);
}

NODISCARD const VkDescriptorImageInfo& VulkanTexture2D::GetDescriptorInfo()
{
    if (m_Image; const auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Image))
    {
        m_DescriptorInfo = vulkanImage->GetDescriptorInfo();
    }
    else
    {
        PFR_ASSERT(false, "VulkanTexture2D: m_Image is not valid! or failed to cast ot VulkanImage!");
    }

    m_DescriptorInfo.sampler = m_Sampler;
    return m_DescriptorInfo;
}

void VulkanTexture2D::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_Index != UINT32_MAX && m_Specification.bBindlessUsage)
    {
        Renderer::GetBindlessRenderer()->FreeTexture(m_Index);
    }

    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    SamplerStorage::DestroySampler(samplerSpec);
}

void VulkanTexture2D::Invalidate(const void* data, const size_t dataSize)
{
    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    m_Sampler                              = (VkSampler)SamplerStorage::CreateOrRetrieveCachedSampler(samplerSpec);

    Texture2D::Invalidate(data, dataSize);

    if (m_Specification.bBindlessUsage)
    {
        const auto& vkTextureInfo = GetDescriptorInfo();
        Renderer::GetBindlessRenderer()->LoadTexture(&vkTextureInfo, m_Index);
    }
}

void VulkanTexture2D::GenerateMipMaps()
{
    // Check if image format supports linear blitting.
    VkFormatProperties formatProperties = {};
    vkGetPhysicalDeviceFormatProperties(VulkanContext::Get().GetDevice()->GetPhysicalDevice(),
                                        ImageUtils::PathfinderImageFormatToVulkan(m_Image->GetSpecification().Format), &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
        !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        PFR_ASSERT(false, "Texture image format does not support linear blitting!");

    auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Image);
    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

    const CommandBufferSpecification cbSpec = {ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS,
                                               ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY, Renderer::GetRendererData()->FrameIndex,
                                               ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
    auto vulkanCommandBuffer =
        MakeShared<VulkanCommandBuffer>(cbSpec);  // Blit is a graphics command, so dedicated transfer queue doesn't support them.
    vulkanCommandBuffer->BeginRecording(true);

    const auto& imageSpec             = m_Image->GetSpecification();
    VkImageBlit regions               = {};
    regions.srcSubresource.aspectMask = ImageUtils::IsDepthFormat(imageSpec.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    regions.srcSubresource.layerCount = imageSpec.Layers;
    regions.srcOffsets[0]             = {0, 0, 0};
    regions.dstSubresource            = regions.srcSubresource;

    int32_t mipWidth = imageSpec.Width, mipHeight = imageSpec.Height;
    VkImage vulkanImageRaw     = (VkImage)vulkanImage->Get();
    const auto prevImageLayout = ImageUtils::PathfinderImageLayoutToVulkan(imageSpec.Layout);
    for (uint32_t mipLevel = 1; mipLevel < imageSpec.Mips; ++mipLevel)
    {
        vulkanCommandBuffer->TransitionImageLayout(vulkanImageRaw, mipLevel == 1 ? prevImageLayout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, regions.srcSubresource.aspectMask,
                                                   regions.srcSubresource.layerCount, 0, 1, mipLevel - 1);

        vulkanCommandBuffer->TransitionImageLayout(vulkanImageRaw, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   regions.srcSubresource.aspectMask, regions.srcSubresource.layerCount, 0, 1, mipLevel);

        regions.srcSubresource.baseArrayLayer = 0;
        regions.srcSubresource.mipLevel       = mipLevel - 1;  // Get previous.
        regions.srcOffsets[1]                 = {mipWidth, mipHeight, 1};

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;

        regions.dstSubresource.baseArrayLayer = 0;
        regions.dstSubresource.mipLevel       = mipLevel;  // Blit previous into current.
        regions.dstOffsets[1]                 = {mipWidth, mipHeight, 1};

        vulkanCommandBuffer->BlitImage(vulkanImageRaw, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vulkanImageRaw,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regions, VK_FILTER_LINEAR);

        vulkanCommandBuffer->TransitionImageLayout(vulkanImageRaw, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, prevImageLayout,
                                                   regions.srcSubresource.aspectMask, regions.srcSubresource.layerCount, 0, 1,
                                                   mipLevel - 1);
    }

    // Last one 1x1 mip is not covered by the loop
    vulkanCommandBuffer->TransitionImageLayout(vulkanImageRaw, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, prevImageLayout,
                                               regions.srcSubresource.aspectMask, regions.srcSubresource.layerCount, 0, 1,
                                               imageSpec.Mips - 1);

    vulkanCommandBuffer->EndRecording();
    vulkanCommandBuffer->Submit()->Wait();
}

}  // namespace Pathfinder