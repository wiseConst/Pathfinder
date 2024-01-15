#include "PathfinderPCH.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "VulkanCommandBuffer.h"
#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{
namespace ImageUtils
{

VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags)
{
    VkImageUsageFlags vkUsageFlags = 0;

    if (usageFlags & EImageUsage::IMAGE_USAGE_SAMPLED_BIT) vkUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT) vkUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_INPUT_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT)
        vkUsageFlags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    if (usageFlags & EImageUsage::IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT) vkUsageFlags |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;

    PFR_ASSERT(vkUsageFlags > 0, "Image should have any usage!");
    return vkUsageFlags;
}

VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat)
{
    switch (imageFormat)
    {
        case EImageFormat::FORMAT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case EImageFormat::FORMAT_RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case EImageFormat::FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case EImageFormat::FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM;
        case EImageFormat::FORMAT_R16F: return VK_FORMAT_R16_SFLOAT;
        case EImageFormat::FORMAT_R32F: return VK_FORMAT_R32_SFLOAT;
        case EImageFormat::FORMAT_R64F: return VK_FORMAT_R64_SFLOAT;
        case EImageFormat::FORMAT_RGB16_UNORM: return VK_FORMAT_R16G16B16_UNORM;
        case EImageFormat::FORMAT_RGB16F: return VK_FORMAT_R16G16B16_SFLOAT;
        case EImageFormat::FORMAT_RGBA16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case EImageFormat::FORMAT_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case EImageFormat::FORMAT_RGB32F: return VK_FORMAT_R32G32B32_SFLOAT;
        case EImageFormat::FORMAT_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EImageFormat::FORMAT_RGB64F: return VK_FORMAT_R64G64B64_SFLOAT;
        case EImageFormat::FORMAT_RGBA64F: return VK_FORMAT_R64G64B64A64_SFLOAT;

        case EImageFormat::FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;
        case EImageFormat::FORMAT_D32F: return VK_FORMAT_D32_SFLOAT;
        case EImageFormat::FORMAT_S8_UINT: return VK_FORMAT_S8_UINT;
        case EImageFormat::FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
        case EImageFormat::FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return VK_FORMAT_UNDEFINED;
}

// NOTE: MultiGPU feature gonna require that device creates images
void CreateImage(VkImage& image, VmaAllocation& allocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D extent, const uint32_t mipLevels)
{
    VkImageCreateInfo imageCI = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCI.extent            = extent;
    imageCI.format            = format;
    imageCI.usage             = imageUsage;
    imageCI.mipLevels         = mipLevels;

    // TODO: How do I handle tis? Hardcoding for now
    // imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCI.imageType     = VK_IMAGE_TYPE_2D;
    imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;  // What if this img gonna be used in dedicated async compute queue and graphics
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.arrayLayers   = 1;
    imageCI.samples       = VK_SAMPLE_COUNT_1_BIT;

    VulkanContext::Get().GetDevice()->GetAllocator()->CreateImage(imageCI, image, allocation);
}

void CreateImageView(const VkImage& image, VkImageView& imageView, const VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType, const uint32_t mipLevels)
{
    VkImageViewCreateInfo imageViewCreateInfo       = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.viewType                    = imageViewType;
    imageViewCreateInfo.image                       = image;
    imageViewCreateInfo.format                      = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;

    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = imageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;

    VK_CHECK(vkCreateImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &imageViewCreateInfo, nullptr, &imageView),
             "Failed to create an image view!");
}

void DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyImage(image, allocation);
}

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout)
{
    switch (imageLayout)
    {
        case EImageLayout::IMAGE_LAYOUT_UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
        case EImageLayout::IMAGE_LAYOUT_GENERAL: return VK_IMAGE_LAYOUT_GENERAL;
        case EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_PRESENT_SRC: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case EImageLayout::IMAGE_LAYOUT_SHARED_PRESENT: return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
        case EImageLayout::IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    }

    PFR_ASSERT(false, "Unknown image layout!");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace ImageUtils

VulkanImage::VulkanImage(const ImageSpecification& imageSpec) : m_Specification(imageSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

void VulkanImage::SetLayout(const EImageLayout newLayout)
{
    auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS);
    vulkanCommandBuffer->BeginRecording(true);

    vulkanCommandBuffer->TransitionImageLayout(
        m_Handle, ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout), ImageUtils::PathfinderImageLayoutToVulkan(newLayout),
        ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    vulkanCommandBuffer->EndRecording();
    vulkanCommandBuffer->Submit(true);

    m_Specification.Layout = newLayout;
}

// TODO: Add cube map support
void VulkanImage::Invalidate()
{
    if (m_Handle) Destroy();

    const auto vkImageFormat = ImageUtils::PathfinderImageFormatToVulkan(m_Specification.Format);
    ImageUtils::CreateImage(m_Handle, m_Allocation, vkImageFormat,
                            ImageUtils::PathfinderImageUsageFlagsToVulkan(m_Specification.UsageFlags),
                            {m_Specification.Width, m_Specification.Height, 1});
    ImageUtils::CreateImageView(m_Handle, m_View, vkImageFormat,
                                ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL);
}

void VulkanImage::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    ImageUtils::DestroyImage(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    vkDestroyImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_View, nullptr);
    m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_UNDEFINED;

    if (m_Index != UINT32_MAX)
    {
        auto vulkanBR = std::static_pointer_cast<VulkanBindlessRenderer>(Renderer::GetBindlessRenderer());
        PFR_ASSERT(vulkanBR, "Failed to cast BindlessRenderer to VulkanBindlessRenderer!");

        vulkanBR->FreeImage(m_Index);
    }
}

}  // namespace Pathfinder