#include "PathfinderPCH.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

namespace Pathfinder
{
namespace ImageUtils
{
void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectFlags,
                     VkImageViewType imageViewType, const uint32_t mipLevels)
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

}  // namespace Pathfinder