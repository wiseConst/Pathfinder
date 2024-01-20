#ifndef VULKANTEXTURE2D_H
#define VULKANTEXTURE2D_H

#include "Renderer/Texture2D.h"
#include "VulkanCore.h"
#include "VulkanImage.h"

namespace Pathfinder
{

class VulkanTexture2D final : public Texture2D
{
  public:
    VulkanTexture2D(const TextureSpecification& textureSpec);
    VulkanTexture2D() = delete;
    ~VulkanTexture2D() override { Destroy(); }

    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const final override { return m_Index; }
    NODISCARD FORCEINLINE const VkDescriptorImageInfo GetDescriptorInfo() const
    {
        const auto imageDescriptorInfo       = m_Image->GetDescriptorInfo();
        VkDescriptorImageInfo descriptorInfo = {m_Sampler, imageDescriptorInfo.imageView, imageDescriptorInfo.imageLayout};
        return descriptorInfo;
    }

  private:
    Shared<VulkanImage> m_Image;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    friend class VulkanBindlessRenderer;
    uint32_t m_Index = UINT32_MAX;  // bindless array purposes

    void Destroy() final override;
};

}  // namespace Pathfinder

#endif