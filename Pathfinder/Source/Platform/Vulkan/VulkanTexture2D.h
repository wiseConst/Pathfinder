#ifndef VULKANTEXTURE2D_H
#define VULKANTEXTURE2D_H

#include "Renderer/Texture2D.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanTexture2D final : public Texture2D
{
  public:
    VulkanTexture2D(const TextureSpecification& textureSpec);
    VulkanTexture2D() = delete;
    ~VulkanTexture2D() override { Destroy(); }

    // NOTE: Since image layout changes frequently, update layout on call.
    NODISCARD const VkDescriptorImageInfo& GetDescriptorInfo();

  private:
    VkDescriptorImageInfo m_DescriptorInfo = {};
    VkSampler m_Sampler                    = VK_NULL_HANDLE;

    friend class VulkanBindlessRenderer;

    void Destroy() final override;
    void Invalidate() final override;
};

}  // namespace Pathfinder

#endif