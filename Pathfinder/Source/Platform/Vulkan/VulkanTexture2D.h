#pragma once

#include "Renderer/Texture2D.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanTexture2D final : public Texture2D
{
  public:
    VulkanTexture2D(const TextureSpecification& textureSpec, const void* data, const size_t dataSize);
    ~VulkanTexture2D() override { Destroy(); }

    // NOTE: Since image layout changes frequently, update layout on call.
    NODISCARD const VkDescriptorImageInfo& GetDescriptorInfo();

  private:
    VkDescriptorImageInfo m_DescriptorInfo = {};
    VkSampler m_Sampler                    = VK_NULL_HANDLE;

    friend class VulkanBindlessRenderer;

    VulkanTexture2D() = delete;
    void Destroy() final override;
    void Invalidate(const void* data, const size_t dataSize) final override;
    void GenerateMipMaps() final override;
};

}  // namespace Pathfinder
