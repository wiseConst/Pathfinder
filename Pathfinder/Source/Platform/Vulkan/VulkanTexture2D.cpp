#include "PathfinderPCH.h"
#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

VulkanTexture2D::VulkanTexture2D(const TextureSpecification& textureSpec) : Texture2D(textureSpec)
{
    Invalidate();
}

NODISCARD const VkDescriptorImageInfo& VulkanTexture2D::GetDescriptorInfo()
{
    if (m_Image; const auto vulkanImage = std::static_pointer_cast<VulkanImage>(m_Image))
    {
        m_DescriptorInfo = vulkanImage->GetDescriptorInfo();
    }
    else
    {
        PFR_ASSERT(false, "VulkanTexture2D: m_Image is not valid! or failed to cast ot VulkanImage!");
    }

    m_DescriptorInfo.sampler = m_Sampler;
    return m_DescriptorInfo;
}

void VulkanTexture2D::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_Index != UINT32_MAX && m_Specification.bBindlessUsage)
    {
        Renderer::GetBindlessRenderer()->FreeTexture(m_Index);
    }

    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    SamplerStorage::DestroySampler(samplerSpec);
}

void VulkanTexture2D::Invalidate()
{
    const SamplerSpecification samplerSpec = {m_Specification.Filter, m_Specification.Wrap};
    m_Sampler                              = (VkSampler)SamplerStorage::CreateOrRetrieveCachedSampler(samplerSpec);

    Texture2D::Invalidate();

    if (m_Specification.bBindlessUsage)
    {
        const auto& vkTextureInfo = GetDescriptorInfo();
        Renderer::GetBindlessRenderer()->LoadTexture(&vkTextureInfo, m_Index);
    }
}

}  // namespace Pathfinder