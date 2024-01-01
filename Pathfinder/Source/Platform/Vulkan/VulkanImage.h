#ifndef VULKANIMAGE_H
#define VULKANIMAGE_H

#include "Renderer/Image.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanImage final : public Image
{
  public:
};

namespace ImageUtils
{

void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectFlags,
                     VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, const uint32_t mipLevels = 1);

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout);

}  // namespace ImageUtils

}  // namespace Pathfinder

#endif  // VULKANIMAGE_H
