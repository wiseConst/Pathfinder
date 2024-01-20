#ifndef VULKANIMAGE_H
#define VULKANIMAGE_H

#include "Renderer/Image.h"
#include "VulkanCore.h"
#include "VulkanAllocator.h"

namespace Pathfinder
{

namespace ImageUtils
{

void CreateImage(VkImage& image, VmaAllocation& allocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D extent, const uint32_t mipLevels = 1);

void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, const uint32_t mipLevels = 1);

void DestroyImage(VkImage& image, VmaAllocation& allocation);

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout);

VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags);

VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat);

}  // namespace ImageUtils

class VulkanImage final : public Image
{
  public:
    VulkanImage(const ImageSpecification& imageSpec);
    VulkanImage() = delete;
    ~VulkanImage() override { Destroy(); }

    NODISCARD FORCEINLINE const ImageSpecification& GetSpecification() const final override { return m_Specification; }
    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE void* GetView() const { return m_View; }
    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const final override { return m_Index; }
    NODISCARD FORCEINLINE const VkDescriptorImageInfo GetDescriptorInfo() const
    {
        VkDescriptorImageInfo descriptorInfo = {VK_NULL_HANDLE, m_View, ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout)};
        return descriptorInfo;
    }

    void SetLayout(const EImageLayout newLayout) final override;

    FORCEINLINE void Resize(const uint32_t width, const uint32_t height) final override
    {
        m_Specification.Width  = width;
        m_Specification.Height = height;

        Invalidate();
    }

  private:
    VkImage m_Handle                   = VK_NULL_HANDLE;
    VmaAllocation m_Allocation         = VK_NULL_HANDLE;
    VkImageView m_View                 = VK_NULL_HANDLE;
    ImageSpecification m_Specification = {};

    friend class VulkanFramebuffer;
    friend class VulkanBindlessRenderer;
    uint32_t m_Index = UINT32_MAX;  // bindless array purposes

    void Invalidate() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANIMAGE_H
