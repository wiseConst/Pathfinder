#pragma once

#include <Renderer/Texture.h>
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanTexture final : public Texture
{
  public:
    VulkanTexture(const TextureSpecification& textureSpec, const void* data, const size_t dataSize);
    ~VulkanTexture() override { Destroy(); }

    // NOTE: Since image layout changes frequently, update layout on call.
    NODISCARD const VkDescriptorImageInfo& GetDescriptorInfo();

    void Resize(const uint32_t width, const uint32_t height)
    {
        if (m_Specification.Width == width && m_Specification.Height == height) return;

        m_Specification.Width  = width;
        m_Specification.Height = height;
        Invalidate(nullptr, 0);

        //  m_Image->Resize(width, height);
        //  m_DescriptorInfo = GetDescriptorInfo();
    }

    void SetDebugName(const std::string& name) final override
    {
        m_Specification.DebugName = name;
        m_Image->SetDebugName(name);
    }

  private:
    VkDescriptorImageInfo m_DescriptorInfo = {};
    VkSampler m_Sampler                    = VK_NULL_HANDLE;

    VulkanTexture() = delete;
    void Destroy() final override;
    void Invalidate(const void* data, const size_t dataSize) final override;
    void GenerateMipMaps() final override;
};

}  // namespace Pathfinder
