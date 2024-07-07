#pragma once

#include <Renderer/Texture.h>
#include "VulkanCore.h"

#include "VulkanAllocator.h"

namespace Pathfinder
{

namespace TextureUtils
{

void CreateImage(VkImage& outImage, VmaAllocation& outAllocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D& extent, const VkImageType imageType = VK_IMAGE_TYPE_2D, const uint32_t mipLevels = 1,
                 const uint32_t layerCount = 1, const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                 const VkImageTiling imageTiling     = VK_IMAGE_TILING_OPTIMAL,
                 const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT) noexcept;

void CreateImageView(const VkImage& image, VkImageView& imageView, VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D, const uint32_t baseMipLevel = 0,
                     const uint32_t mipLevels = 1, const uint32_t baseArrayLayer = 0, const uint32_t layerCount = 1) noexcept;

void DestroyImageView(VkImageView& imageView) noexcept;
void DestroyImage(VkImage& image, VmaAllocation& allocation) noexcept;

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout) noexcept;

VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags) noexcept;

VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat) noexcept;

}  // namespace TextureUtils

// TODO: Vector<ImageView> per mip level
class VulkanTexture final : public Texture
{
  public:
    VulkanTexture(const TextureSpecification& textureSpec, const void* data, const size_t dataSize) noexcept;
    ~VulkanTexture() override { Destroy(); }

    // NOTE: Since image layout changes frequently, update layout on call.
    NODISCARD const VkDescriptorImageInfo GetDescriptorInfo() const noexcept;

    NODISCARD FORCEINLINE void* Get() const noexcept final override { return m_Image; }
    NODISCARD FORCEINLINE void* GetView() const noexcept final override { return m_ImageView; }

    void Invalidate(const void* data, const size_t dataSize) noexcept final override;
    void ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const noexcept final override;
    void Resize(const glm::uvec3& dimensions) noexcept final override
    {
        if (dimensions == m_Specification.Dimensions) return;

        m_Specification.Dimensions = dimensions;

        Invalidate(nullptr, 0);
    }

    void SetLayout(const EImageLayout newLayout, const bool bImmediate = false) noexcept final override;
    void SetDebugName(const std::string& name) noexcept final override;
    void SetData(const void* data, size_t dataSize) noexcept final override;

  private:
    VkSampler m_Sampler        = VK_NULL_HANDLE;
    VkImage m_Image            = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    VkImageView m_ImageView    = VK_NULL_HANDLE;
    //   std::vector<VkImageView> m_MipChain;  // TODO:

    VulkanTexture() = delete;
    void Destroy() noexcept final override;
    void GenerateMipMaps() noexcept final override;
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
