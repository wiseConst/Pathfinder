#include "PathfinderPCH.h"
#include "Texture2D.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

namespace Pathfinder
{
Shared<Texture2D> Texture2D::Create(const TextureSpecification& textureSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanTexture2D>(textureSpec);
    }

    PFR_ASSERT(false, "Unknown Renderer API!");
    return nullptr;
}

void Texture2D::Invalidate()
{
    if (m_Image)
    {
        PFR_ASSERT(false, "No texture invalidation implemented && tested!");
        return;
    }

    ImageSpecification imageSpec = {m_Specification.Width, m_Specification.Height};
    imageSpec.Format             = m_Specification.Format;
    imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;
    imageSpec.Layers             = m_Specification.Layers;

    m_Image = Image::Create(imageSpec);

    m_Image->SetData(m_Specification.Data, m_Specification.DataSize);
    m_Specification.Data     = nullptr;
    m_Specification.DataSize = 0;

    m_Image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}  // namespace Pathfinder