#pragma once

#include "Renderer/Image.h"
#include "VulkanCore.h"
#include "VulkanAllocator.h"

namespace Pathfinder
{

namespace ImageUtils
{

void CreateImage(VkImage& outImage, VmaAllocation& outAllocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D& extent, const VkImageType imageType = VK_IMAGE_TYPE_2D, const uint32_t mipLevels = 1,
                 const uint32_t layerCount = 1, const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                 const VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL, const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, const uint32_t baseMipLevel = 0,
                     const uint32_t mipLevels = 1, const uint32_t baseArrayLayer = 0, const uint32_t layerCount = 1);

void DestroyImageView(VkImageView& imageView);
void DestroyImage(VkImage& image, VmaAllocation& allocation);

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout);

VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags);

VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat);

}  // namespace ImageUtils

// TODO: Vector<ImageView> per mip level
class VulkanImage final : public Image
{
  public:
    VulkanImage(const ImageSpecification& imageSpec);
    ~VulkanImage() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE const auto& GetView() const { return m_View; }
    NODISCARD FORCEINLINE const auto& GetDescriptorInfo()
    {
        m_DescriptorInfo.imageLayout = ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout);
        return m_DescriptorInfo;
    }
    NODISCARD FORCEINLINE const auto& GetSampler() const
    {
        if (!m_DescriptorInfo.sampler) LOG_WARN("Returning null image sampler! (This shouldn't happen.)");
        return m_DescriptorInfo.sampler;
    }

    void SetLayout(const EImageLayout newLayout, const bool bImmediate = false) final override;
    void SetData(const void* data, size_t dataSize) final override;
    void ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const final override;
    void SetDebugName(const std::string& name) final override;

    FORCEINLINE void Resize(const uint32_t width, const uint32_t height) final override
    {
        if (m_Specification.Width == width && m_Specification.Height == height) return;

        m_Specification.Width  = width;
        m_Specification.Height = height;

        Invalidate();
    }

  private:
    VkImage m_Handle                       = VK_NULL_HANDLE;
    VmaAllocation m_Allocation             = VK_NULL_HANDLE;
    VkImageView m_View                     = VK_NULL_HANDLE;
    VkDescriptorImageInfo m_DescriptorInfo = {};
 //   std::vector<VkImageView> m_ViewMips;  // TODO:

    VulkanImage() = delete;
    void Invalidate() final override;
    void Destroy() final override;
};

class VulkanSamplerStorage final : public SamplerStorage
{
  public:
    VulkanSamplerStorage()  = default;
    ~VulkanSamplerStorage() = default;

    void* CreateSamplerImpl(const SamplerSpecification& samplerSpec) final override;
    void DestroySamplerImpl(void* sampler) final override;
};

}  // namespace Pathfinder
