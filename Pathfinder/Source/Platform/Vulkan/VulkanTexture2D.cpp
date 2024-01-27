#include "PathfinderPCH.h"
#include "VulkanTexture2D.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

NODISCARD static ESamplerFilter VulkanSamplerFilterToPathfinder(const VkFilter filter)
{
    switch (filter)
    {
        case VK_FILTER_NEAREST: return ESamplerFilter::SAMPLER_FILTER_NEAREST;
        case VK_FILTER_LINEAR: return ESamplerFilter::SAMPLER_FILTER_LINEAR;
    }

    PFR_ASSERT(false, "Unknown sampler filter!");
    return ESamplerFilter::SAMPLER_FILTER_LINEAR;
}

NODISCARD static ESamplerWrap VulkanSamplerWrapToPathfinder(const VkSamplerAddressMode wrap)
{
    switch (wrap)
    {
        case VK_SAMPLER_ADDRESS_MODE_REPEAT: return ESamplerWrap::SAMPLER_WRAP_REPEAT;
        case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_BORDER;
        case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return ESamplerWrap::SAMPLER_WRAP_MIRROR_CLAMP_TO_EDGE;
    }

    PFR_ASSERT(false, "Unknown sampler wrap!");
    return ESamplerWrap::SAMPLER_WRAP_REPEAT;
}

NODISCARD static ECompareOp VulkanCompareOpToPathfinder(const VkCompareOp compareOp)
{
    switch (compareOp)
    {
        case VK_COMPARE_OP_NEVER: return ECompareOp::COMPARE_OP_NEVER;
        case VK_COMPARE_OP_LESS: return ECompareOp::COMPARE_OP_LESS;
        case VK_COMPARE_OP_EQUAL: return ECompareOp::COMPARE_OP_EQUAL;
        case VK_COMPARE_OP_LESS_OR_EQUAL: return ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
        case VK_COMPARE_OP_GREATER: return ECompareOp::COMPARE_OP_GREATER;
        case VK_COMPARE_OP_NOT_EQUAL: return ECompareOp::COMPARE_OP_NOT_EQUAL;
        case VK_COMPARE_OP_GREATER_OR_EQUAL: return ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
        case VK_COMPARE_OP_ALWAYS: return ECompareOp::COMPARE_OP_ALWAYS;
    }

    PFR_ASSERT(false, "Unknown sampler wrap!");
    return ECompareOp::COMPARE_OP_NEVER;
}

static SamplerSpecification VulkanSamplerCreateInfoToPathfinder(const VkSamplerCreateInfo samplerCI)
{
    SamplerSpecification samplerSpec = {};
    samplerSpec.Filter               = VulkanSamplerFilterToPathfinder(samplerCI.magFilter);
    samplerSpec.Wrap                 = VulkanSamplerWrapToPathfinder(samplerCI.addressModeU);
    samplerSpec.bAnisotropyEnable    = samplerCI.anisotropyEnable;
    samplerSpec.bCompareEnable       = samplerCI.compareEnable;
    samplerSpec.MipLodBias           = samplerCI.mipLodBias;
    samplerSpec.MaxAnisotropy        = samplerCI.maxAnisotropy;
    samplerSpec.MinLod               = samplerCI.minLod;
    samplerSpec.MaxLod               = samplerCI.maxLod;
    samplerSpec.CompareOp            = VulkanCompareOpToPathfinder(samplerCI.compareOp);

    return samplerSpec;
}

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

    {
        // Sampler management
        VkSamplerCreateInfo samplerCI     = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.unnormalizedCoordinates = VK_FALSE;
        samplerCI.addressModeU            = VulkanUtility::PathfinderSamplerWrapToVulkan(m_Specification.Wrap);
        samplerCI.addressModeV            = samplerCI.addressModeU;
        samplerCI.addressModeW            = samplerCI.addressModeU;

        samplerCI.magFilter = VulkanUtility::PathfinderSamplerFilterToVulkan(m_Specification.Filter);
        samplerCI.minFilter = VulkanUtility::PathfinderSamplerFilterToVulkan(m_Specification.Filter);

        samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;  // TODO: configurable??
        samplerCI.mipmapMode  = samplerCI.magFilter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;

        const SamplerSpecification samplerSpec = VulkanSamplerCreateInfoToPathfinder(samplerCI);
        if (SamplerStorage::DoesSamplerNeedDestruction(samplerSpec))
        {
            vkDestroySampler(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Sampler, nullptr);
            SamplerStorage::DestroySampler(samplerSpec);
        }
        else
            SamplerStorage::DecrementSamplerImageUsage(samplerSpec);
    }
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

    {
        // Sampler management
        VkSamplerCreateInfo samplerCI     = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerCI.unnormalizedCoordinates = VK_FALSE;
        samplerCI.addressModeU            = VulkanUtility::PathfinderSamplerWrapToVulkan(m_Specification.Wrap);
        samplerCI.addressModeV            = samplerCI.addressModeU;
        samplerCI.addressModeW            = samplerCI.addressModeU;

        samplerCI.magFilter = VulkanUtility::PathfinderSamplerFilterToVulkan(m_Specification.Filter);
        samplerCI.minFilter = VulkanUtility::PathfinderSamplerFilterToVulkan(m_Specification.Filter);

        samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;  // TODO: configurable??
        samplerCI.mipmapMode  = samplerCI.magFilter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;

        const SamplerSpecification samplerSpec = VulkanSamplerCreateInfoToPathfinder(samplerCI);
        if (void* rawSampler = SamplerStorage::RetrieveCachedSampler(samplerSpec))
        {
            m_Sampler = (VkSampler)rawSampler;
        }
        else
        {
            VK_CHECK(vkCreateSampler(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &samplerCI, nullptr, &m_Sampler),
                     "Failed to create vulkan sampler!");
            SamplerStorage::AddNewSampler(samplerSpec, m_Sampler, 1);
        }
    }
}

}  // namespace Pathfinder