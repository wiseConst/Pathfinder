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

    NODISCARD FORCEINLINE const TextureSpecification& GetSpecification() const final override { return m_Specification; }
    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const final override { return m_Index; }

    // NOTE: Since image layout changes frequently it's done like this:
    NODISCARD FORCEINLINE const auto& GetDescriptorInfo()
    {
        m_DescriptorInfo         = m_Image->GetDescriptorInfo();
        m_DescriptorInfo.sampler = m_Sampler;
        return m_DescriptorInfo;
    }

  private:
    TextureSpecification m_Specification = {};
    Shared<VulkanImage> m_Image;
    VkDescriptorImageInfo m_DescriptorInfo = {};
    VkSampler m_Sampler                    = VK_NULL_HANDLE;

    friend class VulkanBindlessRenderer;
    uint32_t m_Index = UINT32_MAX;  // bindless array purposes

    void Destroy() final override;
    void Invalidate() final override;
};

}  // namespace Pathfinder

#endif