#include "PathfinderPCH.h"
#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

VulkanTexture2D::VulkanTexture2D(const TextureSpecification& textureSpec) : m_Specification(textureSpec)
{
    Invalidate();
}

void VulkanTexture2D::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_Index != UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->FreeTexture(m_Index);
    }

    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    SamplerStorage::DestroySampler(samplerSpec);
}

// TODO: Move it into Texture2D since abstractions are good enough
void VulkanTexture2D::Invalidate()
{
    ImageSpecification imageSpec = {};
    imageSpec.Height             = m_Specification.Height;
    imageSpec.Width              = m_Specification.Width;
    imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;  // TODO: What format to choose? I don't remember...
    imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;

    m_Image = MakeShared<VulkanImage>(imageSpec);

    m_Image->SetData(m_Specification.Data, m_Specification.DataSize);
    m_Specification.Data     = nullptr;
    m_Specification.DataSize = 0;

    m_Image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    m_Sampler                              = (VkSampler)SamplerStorage::CreateOrRetrieveCachedSampler(samplerSpec);
}

}  // namespace Pathfinder