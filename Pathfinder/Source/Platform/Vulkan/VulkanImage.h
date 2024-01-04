#ifndef VULKANIMAGE_H
#define VULKANIMAGE_H

#include "Renderer/Image.h"
#include "VulkanCore.h"
#include "VulkanAllocator.h"

namespace Pathfinder
{

class VulkanImage final : public Image
{
  public:
  private:
    VkImage m_Handle           = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    ImageSpecification m_Specification;
};

namespace ImageUtils
{

void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, VkImageAspectFlags aspectFlags,
                     VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, const uint32_t mipLevels = 1);

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout);

}  // namespace ImageUtils

}  // namespace Pathfinder

#endif  // VULKANIMAGE_H
