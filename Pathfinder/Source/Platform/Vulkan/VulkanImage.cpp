#include "PathfinderPCH.h"
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanBuffer.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{
namespace ImageUtils
{

VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags)
{
    VkImageUsageFlags vkUsageFlags = 0;

    if (usageFlags & EImageUsage::IMAGE_USAGE_SAMPLED_BIT) vkUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT) vkUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_INPUT_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT)
        vkUsageFlags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
    if (usageFlags & EImageUsage::IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT) vkUsageFlags |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;

    PFR_ASSERT(vkUsageFlags > 0, "Image should have any usage!");
    return vkUsageFlags;
}

VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat)
{
    switch (imageFormat)
    {
        case EImageFormat::FORMAT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case EImageFormat::FORMAT_RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case EImageFormat::FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case EImageFormat::FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
        case EImageFormat::FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM;
        case EImageFormat::FORMAT_R16F: return VK_FORMAT_R16_SFLOAT;
        case EImageFormat::FORMAT_R32F: return VK_FORMAT_R32_SFLOAT;
        case EImageFormat::FORMAT_R64F: return VK_FORMAT_R64_SFLOAT;
        case EImageFormat::FORMAT_RGB16_UNORM: return VK_FORMAT_R16G16B16_UNORM;
        case EImageFormat::FORMAT_RGB16F: return VK_FORMAT_R16G16B16_SFLOAT;
        case EImageFormat::FORMAT_RGBA16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case EImageFormat::FORMAT_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case EImageFormat::FORMAT_RGB32F: return VK_FORMAT_R32G32B32_SFLOAT;
        case EImageFormat::FORMAT_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EImageFormat::FORMAT_RGB64F: return VK_FORMAT_R64G64B64_SFLOAT;
        case EImageFormat::FORMAT_RGBA64F: return VK_FORMAT_R64G64B64A64_SFLOAT;

        case EImageFormat::FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;
        case EImageFormat::FORMAT_D32F: return VK_FORMAT_D32_SFLOAT;
        case EImageFormat::FORMAT_S8_UINT: return VK_FORMAT_S8_UINT;
        case EImageFormat::FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
        case EImageFormat::FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return VK_FORMAT_UNDEFINED;
}

// NOTE: MultiGPU feature gonna require that device creates images
void CreateImage(VkImage& image, VmaAllocation& allocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D extent, const uint32_t mipLevels)
{
    VkImageCreateInfo imageCI = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCI.imageType         = VK_IMAGE_TYPE_2D;
    imageCI.extent            = extent;
    imageCI.format            = format;
    imageCI.usage             = imageUsage;
    imageCI.mipLevels         = mipLevels;

    // NOTE: No sharing between queues
    imageCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.queueFamilyIndexCount = 0;
    imageCI.pQueueFamilyIndices   = nullptr;

    // TODO: How do I handle tis? Hardcoding for now
    // imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.arrayLayers   = 1;
    imageCI.samples       = VK_SAMPLE_COUNT_1_BIT;

    VulkanContext::Get().GetDevice()->GetAllocator()->CreateImage(imageCI, image, allocation);
}

void CreateImageView(const VkImage& image, VkImageView& imageView, const VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType, const uint32_t mipLevels)
{
    VkImageViewCreateInfo imageViewCreateInfo       = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.viewType                    = imageViewType;
    imageViewCreateInfo.image                       = image;
    imageViewCreateInfo.format                      = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;

    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = mipLevels;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = imageViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;

    VK_CHECK(vkCreateImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &imageViewCreateInfo, nullptr, &imageView),
             "Failed to create an image view!");
}

void DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyImage(image, allocation);
}

VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout)
{
    switch (imageLayout)
    {
        case EImageLayout::IMAGE_LAYOUT_UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
        case EImageLayout::IMAGE_LAYOUT_GENERAL: return VK_IMAGE_LAYOUT_GENERAL;
        case EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_PRESENT_SRC: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case EImageLayout::IMAGE_LAYOUT_SHARED_PRESENT: return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
        case EImageLayout::IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    }

    PFR_ASSERT(false, "Unknown image layout!");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace ImageUtils

VulkanImage::VulkanImage(const ImageSpecification& imageSpec) : m_Specification(imageSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

void VulkanImage::SetLayout(const EImageLayout newLayout)
{
    auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS);
    vulkanCommandBuffer->BeginRecording(true);

    vulkanCommandBuffer->TransitionImageLayout(
        m_Handle, ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout), ImageUtils::PathfinderImageLayoutToVulkan(newLayout),
        ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    vulkanCommandBuffer->EndRecording();
    vulkanCommandBuffer->Submit(true);

    m_Specification.Layout       = newLayout;
    m_DescriptorInfo.imageLayout = ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout);
}

void VulkanImage::SetData(const void* data, size_t dataSize)
{
    SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const auto& rd = Renderer::GetRendererData();
    rd->UploadHeap[rd->FrameIndex]->SetData(data, dataSize);

    VkBufferImageCopy copyRegion               = {};
    copyRegion.imageExtent                     = {m_Specification.Width, m_Specification.Height, 1};
    copyRegion.imageSubresource.mipLevel       = 0;  // ???
    copyRegion.imageSubresource.baseArrayLayer = 0;  // ???
    copyRegion.imageSubresource.layerCount     = 1;  // ???
    copyRegion.imageSubresource.aspectMask =
        ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

#if TODO
    if (auto commandBuffer = rd->CurrentTransferCommandBuffer.lock())
    {
        auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
        PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

        vulkanCommandBuffer->CopyBufferToImage((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle,
                                               ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout), 1, &copyRegion);
    }
    else
#endif
    {
        auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER);
        vulkanCommandBuffer->BeginRecording(true);

        vulkanCommandBuffer->CopyBufferToImage((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle,
                                               ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout), 1, &copyRegion);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit(true, false);
    }
}

// TODO: Add cube map support
void VulkanImage::Invalidate()
{
    if (m_Handle) Destroy();

    const auto vkImageFormat = ImageUtils::PathfinderImageFormatToVulkan(m_Specification.Format);
    ImageUtils::CreateImage(m_Handle, m_Allocation, vkImageFormat,
                            ImageUtils::PathfinderImageUsageFlagsToVulkan(m_Specification.UsageFlags),
                            {m_Specification.Width, m_Specification.Height, 1});
    ImageUtils::CreateImageView(m_Handle, m_View, vkImageFormat,
                                ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

    // NOTE: Small crutch since SetLayout() doesn't assume using inside Invalidate() but I find it convenient.
    // On image creation it has undefined layout. Store newLayout and set it to specification after transition.
    // Because SetLayout uses oldLayout as m_Specification.Layout and newLayout I specify as m_Specification.Layout,
    // so layouts are equal and no transition happens.
    {
        const EImageLayout newLayout =
            m_Specification.Layout == EImageLayout::IMAGE_LAYOUT_UNDEFINED ? EImageLayout::IMAGE_LAYOUT_GENERAL : m_Specification.Layout;

        m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
        SetLayout(newLayout);
        m_Specification.Layout = newLayout;
    }

    m_DescriptorInfo = {
        (VkSampler)SamplerStorage::CreateOrRetrieveCachedSampler(SamplerSpecification{m_Specification.Filter, m_Specification.Wrap}),
        m_View, ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout)};
}

void VulkanImage::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    ImageUtils::DestroyImage(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    vkDestroyImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_View, nullptr);

    if (m_DescriptorInfo.sampler) SamplerStorage::DestroySampler(SamplerSpecification{m_Specification.Filter, m_Specification.Wrap});

    m_DescriptorInfo = {};

    if (m_Index != UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->FreeImage(m_Index);
    }
}

void VulkanImage::ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    const EImageLayout prevLayout = m_Specification.Layout;
    const auto vkOldLayout        = ImageUtils::PathfinderImageLayoutToVulkan(prevLayout);
    const auto vkNewLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    const auto aspectMask = ImageUtils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    vulkanCommandBuffer->TransitionImageLayout(m_Handle, vkOldLayout, vkNewLayout, aspectMask);

    const VkClearColorValue clearColorValue = {color.x, color.y, color.z, color.w};
    const VkImageSubresourceRange range     = {aspectMask, 0, 1, 0, 1};

    const auto rawCommandBuffer = static_cast<VkCommandBuffer>(commandBuffer->Get());
    vkCmdClearColorImage(rawCommandBuffer, m_Handle, vkNewLayout, &clearColorValue, 1, &range);

    vulkanCommandBuffer->TransitionImageLayout(m_Handle, vkNewLayout, vkOldLayout, aspectMask);
}

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

void* VulkanSamplerStorage::CreateSamplerImpl(const SamplerSpecification& samplerSpec)
{
    VkSamplerCreateInfo samplerCI     = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCI.unnormalizedCoordinates = VK_FALSE;
    samplerCI.addressModeU            = VulkanUtility::PathfinderSamplerWrapToVulkan(samplerSpec.Wrap);
    samplerCI.addressModeV            = samplerCI.addressModeU;
    samplerCI.addressModeW            = samplerCI.addressModeU;

    samplerCI.minLod     = samplerSpec.MinLod;
    samplerCI.maxLod     = IsNearlyEqual(samplerSpec.MaxLod, .0f) ? VK_LOD_CLAMP_NONE : samplerSpec.MaxLod;
    samplerCI.mipLodBias = samplerSpec.MipLodBias;

    samplerCI.minFilter = samplerCI.magFilter = VulkanUtility::PathfinderSamplerFilterToVulkan(samplerSpec.Filter);

    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;  // VK_BORDER_COLOR_INT_OPAQUE_BLACK;  // TODO: configurable??
    samplerCI.mipmapMode  = samplerCI.magFilter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &samplerCI, nullptr, &sampler),
             "Failed to create vulkan sampler!");
    return sampler;
}

void VulkanSamplerStorage::DestroySamplerImpl(void* sampler)
{
    PFR_ASSERT(sampler, "Not valid sampler to destroy!");
    vkDestroySampler(VulkanContext::Get().GetDevice()->GetLogicalDevice(), (VkSampler)sampler, nullptr);
}

}  // namespace Pathfinder