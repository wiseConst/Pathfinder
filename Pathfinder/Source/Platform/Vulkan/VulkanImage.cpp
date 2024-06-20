#include <PathfinderPCH.h>
#include "VulkanImage.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanBuffer.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Renderer.h>

namespace Pathfinder
{
namespace ImageUtils
{
NODISCARD FORCEINLINE static VkSamplerReductionMode PathfinderSamplerReductionModeToVulkan(const ESamplerReductionMode reductionMode)
{
    switch (reductionMode)
    {
        case ESamplerReductionMode::SAMPLER_REDUCTION_MODE_NONE:
        case ESamplerReductionMode::SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
        case ESamplerReductionMode::SAMPLER_REDUCTION_MODE_MIN: return VK_SAMPLER_REDUCTION_MODE_MIN;
        case ESamplerReductionMode::SAMPLER_REDUCTION_MODE_MAX: return VK_SAMPLER_REDUCTION_MODE_MAX;
    }

    PFR_ASSERT(false, "Unknown reduction mode!");
    return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
}

NODISCARD FORCEINLINE static VkFilter PathfinderSamplerFilterToVulkan(const ESamplerFilter filter)
{
    switch (filter)
    {
        case ESamplerFilter::SAMPLER_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case ESamplerFilter::SAMPLER_FILTER_LINEAR: return VK_FILTER_LINEAR;
    }

    PFR_ASSERT(false, "Unknown sampler filter!");
    return VK_FILTER_LINEAR;
}

NODISCARD FORCEINLINE static VkSamplerAddressMode PathfinderSamplerWrapToVulkan(const ESamplerWrap wrap)
{
    switch (wrap)
    {
        case ESamplerWrap::SAMPLER_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case ESamplerWrap::SAMPLER_WRAP_MIRROR_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }

    PFR_ASSERT(false, "Unknown sampler wrap!");
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

NODISCARD VkImageUsageFlags PathfinderImageUsageFlagsToVulkan(const ImageUsageFlags usageFlags)
{
    VkImageUsageFlags vkUsageFlags = 0;

    if (usageFlags & EImageUsage::IMAGE_USAGE_SAMPLED_BIT) vkUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT) vkUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT) vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) vkUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (usageFlags & EImageUsage::IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT)
        vkUsageFlags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

    PFR_ASSERT(vkUsageFlags > 0, "Image should have any usage!");
    return vkUsageFlags;
}

NODISCARD VkFormat PathfinderImageFormatToVulkan(const EImageFormat imageFormat)
{
    switch (imageFormat)
    {
        case EImageFormat::FORMAT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case EImageFormat::FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
        case EImageFormat::FORMAT_RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case EImageFormat::FORMAT_RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case EImageFormat::FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case EImageFormat::FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
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
        case EImageFormat::FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

        case EImageFormat::FORMAT_BC1_RGB_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC1_RGB_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case EImageFormat::FORMAT_BC1_RGBA_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC1_RGBA_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case EImageFormat::FORMAT_BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC2_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
        case EImageFormat::FORMAT_BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
        case EImageFormat::FORMAT_BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
        case EImageFormat::FORMAT_BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
        case EImageFormat::FORMAT_BC6H_UFLOAT: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case EImageFormat::FORMAT_BC6H_SFLOAT: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case EImageFormat::FORMAT_BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        case EImageFormat::FORMAT_BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return VK_FORMAT_UNDEFINED;
}

// NOTE: VK_IMAGE_TILING_LINEAR should never be used and will never be faster.
void CreateImage(VkImage& outImage, VmaAllocation& outAllocation, const VkFormat format, const VkImageUsageFlags imageUsage,
                 const VkExtent3D& extent, const VkImageType imageType, const uint32_t mipLevels, const uint32_t layerCount,
                 const VkImageLayout initialLayout, const VkImageTiling imageTiling, const VkSampleCountFlagBits samples)
{
    const auto& device                        = VulkanContext::Get().GetDevice();
    auto& queueFamilyIndices                  = device->GetQueueFamilyIndices();
    const VkImageCreateFlags imageCreateFlags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    const VkImageCreateInfo imageCI           = {.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                 .flags                 = imageCreateFlags,
                                                 .imageType             = imageType,
                                                 .format                = format,
                                                 .extent                = extent,
                                                 .mipLevels             = mipLevels,
                                                 .arrayLayers           = layerCount,
                                                 .samples               = samples,
                                                 .tiling                = imageTiling,
                                                 .usage                 = imageUsage,
                                                 .sharingMode           = VK_SHARING_MODE_CONCURRENT,
                                                 .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
                                                 .pQueueFamilyIndices   = queueFamilyIndices.data(),
                                                 .initialLayout         = initialLayout};

    device->GetAllocator()->CreateImage(imageCI, outImage, outAllocation);
}

// TODO: ImageViewCache from LegitEngine
void CreateImageView(const VkImage& image, VkImageView& imageView, const VkFormat format, const VkImageAspectFlags aspectFlags,
                     const VkImageViewType imageViewType, const uint32_t baseMipLevel, const uint32_t mipLevels,
                     const uint32_t baseArrayLayer, const uint32_t layerCount)
{
    const VkImageViewCreateInfo imageViewCI = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = image,
        .viewType         = imageViewType,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {.aspectMask     = aspectFlags,
                             .baseMipLevel   = baseMipLevel,
                             .levelCount     = mipLevels,
                             .baseArrayLayer = baseArrayLayer,
                             .layerCount     = layerCount}};

    VK_CHECK(vkCreateImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &imageViewCI, nullptr, &imageView),
             "Failed to create an image view!");

    ++Renderer::GetStats().ImageViewCount;
}

void DestroyImageView(VkImageView& imageView)
{
    vkDestroyImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), imageView, nullptr);
    imageView = VK_NULL_HANDLE;
    --Renderer::GetStats().ImageViewCount;
}

void DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyImage(image, allocation);
}

NODISCARD VkImageLayout PathfinderImageLayoutToVulkan(const EImageLayout imageLayout)
{
    switch (imageLayout)
    {
        case EImageLayout::IMAGE_LAYOUT_UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
        case EImageLayout::IMAGE_LAYOUT_GENERAL: return VK_IMAGE_LAYOUT_GENERAL;
        case EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case EImageLayout::IMAGE_LAYOUT_PRESENT_SRC: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        case EImageLayout::IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    }

    PFR_ASSERT(false, "Unknown image layout!");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace ImageUtils

VulkanImage::VulkanImage(const ImageSpecification& imageSpec) : Image(imageSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

void VulkanImage::SetLayout(const EImageLayout newLayout, const bool bImmediate)
{
    if (bImmediate)
    {
        const CommandBufferSpecification cbSpec = {.Type       = ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL,
                                                   .Level      = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                                                   .FrameIndex = Renderer::GetRendererData()->FrameIndex,
                                                   .ThreadID   = ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        auto vulkanCommandBuffer                = MakeShared<VulkanCommandBuffer>(cbSpec);
        vulkanCommandBuffer->BeginRecording(true);

        VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
        if (ImageUtils::IsStencilFormat(m_Specification.Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (ImageUtils::IsDepthFormat(m_Specification.Format))
            imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

        vulkanCommandBuffer->TransitionImageLayout(m_Handle, ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout),
                                                   ImageUtils::PathfinderImageLayoutToVulkan(newLayout), imageAspectMask,
                                                   m_Specification.Layers, 0, m_Specification.Mips, 0);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit()->Wait();
    }

    m_Specification.Layout = newLayout;
}

void VulkanImage::SetData(const void* data, size_t dataSize)
{
    SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);

    const auto& rd = Renderer::GetRendererData();
    rd->UploadHeap[rd->FrameIndex]->SetData(data, dataSize);

    VkBufferImageCopy copyRegion               = {};
    copyRegion.imageExtent                     = {m_Specification.Width, m_Specification.Height, 1};
    copyRegion.imageSubresource.mipLevel       = 0;  // which mip we do fill
    copyRegion.imageSubresource.baseArrayLayer = 0;  // which layer we do fill
    copyRegion.imageSubresource.layerCount     = m_Specification.Layers;

    VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
    if (ImageUtils::IsStencilFormat(m_Specification.Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (ImageUtils::IsDepthFormat(m_Specification.Format))
        imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.aspectMask = imageAspectMask;

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
        const CommandBufferSpecification cbSpec = {
            .Type       = ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL /*COMMAND_BUFFER_TYPE_TRANSFER_ASYNC*/,
            .Level      = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
            .FrameIndex = Renderer::GetRendererData()->FrameIndex,
            .ThreadID   = ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(cbSpec);
        vulkanCommandBuffer->BeginRecording(true);

        vulkanCommandBuffer->CopyBufferToImage((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle,
                                               ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout), 1, &copyRegion);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit()->Wait();
    }
}

void VulkanImage::Invalidate()
{
    if (m_Handle) Destroy();

    PFR_ASSERT(m_Specification.Mips > 0, "Mips should be 1 at least!");
    const auto vkImageFormat = ImageUtils::PathfinderImageFormatToVulkan(m_Specification.Format);
    ImageUtils::CreateImage(
        m_Handle, m_Allocation, vkImageFormat, ImageUtils::PathfinderImageUsageFlagsToVulkan(m_Specification.UsageFlags),
        {m_Specification.Width, m_Specification.Height, 1}, VK_IMAGE_TYPE_2D, m_Specification.Mips, m_Specification.Layers);

    const VkImageViewType imageViewType = m_Specification.Layers == 1
                                              ? VK_IMAGE_VIEW_TYPE_2D
                                              : (m_Specification.Layers == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D_ARRAY);

    VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
    if (ImageUtils::IsStencilFormat(m_Specification.Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (ImageUtils::IsDepthFormat(m_Specification.Format))
        imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

    ImageUtils::CreateImageView(m_Handle, m_View, vkImageFormat, imageAspectMask, imageViewType, 0, m_Specification.Mips, 0,
                                m_Specification.Layers);

    // NOTE: Small crutch since SetLayout() doesn't assume using inside Invalidate() but I find it convenient.
    // On image creation it has undefined layout. We store newLayout and set it to specification after transition.
    // Because SetLayout uses oldLayout as m_Specification.Layout and newLayout I specify as m_Specification.Layout,
    // so layouts are equal and no transition happens.
    {
        const EImageLayout newLayout =
            m_Specification.Layout == EImageLayout::IMAGE_LAYOUT_UNDEFINED ? EImageLayout::IMAGE_LAYOUT_GENERAL : m_Specification.Layout;

        m_Specification.Layout = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
        SetLayout(newLayout, true);
        m_Specification.Layout = newLayout;
    }

    if (m_Specification.UsageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT && !m_BindlessIndex.has_value())
    {
        SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL, true);
        const VkDescriptorImageInfo vkImageInfo = {.imageView   = m_View,
                                                   .imageLayout = ImageUtils::PathfinderImageLayoutToVulkan(m_Specification.Layout)};
        Renderer::GetDescriptorManager()->LoadImage(&vkImageInfo, m_BindlessIndex);
    }

    if (m_Specification.DebugName != s_DEFAULT_STRING)
    {
        VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_IMAGE,
                        m_Specification.DebugName.data());
    }
}

void VulkanImage::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    ImageUtils::DestroyImage(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    ImageUtils::DestroyImageView(m_View);
    vkDestroyImageView(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_View, nullptr);

    if (m_Specification.UsageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT && m_BindlessIndex.has_value())
        Renderer::GetDescriptorManager()->FreeImage(m_BindlessIndex);
}

void VulkanImage::ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    const EImageLayout prevLayout = m_Specification.Layout;
    const auto vkOldLayout        = ImageUtils::PathfinderImageLayoutToVulkan(prevLayout);
    const auto vkNewLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkImageAspectFlags imageAspectMask = VK_IMAGE_ASPECT_NONE;
    if (ImageUtils::IsStencilFormat(m_Specification.Format)) imageAspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (ImageUtils::IsDepthFormat(m_Specification.Format))
        imageAspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        imageAspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

    vulkanCommandBuffer->TransitionImageLayout(m_Handle, vkOldLayout, vkNewLayout, imageAspectMask, m_Specification.Layers, 0,
                                               m_Specification.Mips, 0);

    const VkClearColorValue clearColorValue = {color.x, color.y, color.z, color.w};
    const VkImageSubresourceRange range     = {.aspectMask     = imageAspectMask,
                                               .baseMipLevel   = 0,
                                               .levelCount     = m_Specification.Mips,
                                               .baseArrayLayer = 0,
                                               .layerCount     = m_Specification.Layers};

    const auto rawCommandBuffer = static_cast<VkCommandBuffer>(commandBuffer->Get());
    vkCmdClearColorImage(rawCommandBuffer, m_Handle, vkNewLayout, &clearColorValue, 1, &range);

    vulkanCommandBuffer->TransitionImageLayout(m_Handle, vkNewLayout, vkOldLayout, imageAspectMask, m_Specification.Layers, 0,
                                               m_Specification.Mips, 0);
}

void VulkanImage::SetDebugName(const std::string& name)
{
    m_Specification.DebugName = name;
    VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_IMAGE, m_Specification.DebugName.data());
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
    const auto& device     = VulkanContext::Get().GetDevice();
    const auto addressMode = ImageUtils::PathfinderSamplerWrapToVulkan(samplerSpec.Wrap);
    const auto filter      = ImageUtils::PathfinderSamplerFilterToVulkan(samplerSpec.Filter);

    const VkSamplerReductionModeCreateInfo reductionModeCI = {
        .sType         = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
        .reductionMode = ImageUtils::PathfinderSamplerReductionModeToVulkan(samplerSpec.ReductionMode)};

    const VkSamplerCreateInfo samplerCI = {
        .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext            = samplerSpec.ReductionMode == ESamplerReductionMode::SAMPLER_REDUCTION_MODE_NONE ? nullptr : &reductionModeCI,
        .magFilter        = filter,
        .minFilter        = filter,
        .mipmapMode       = filter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU     = addressMode,
        .addressModeV     = addressMode,
        .addressModeW     = addressMode,
        .mipLodBias       = samplerSpec.MipLodBias,
        .anisotropyEnable = samplerSpec.bAnisotropyEnable ? VK_TRUE : VK_FALSE,
        .maxAnisotropy    = samplerSpec.bAnisotropyEnable ? device->GetMaxSamplerAnisotropy() : 0.f,
        .compareEnable    = samplerSpec.bCompareEnable ? VK_TRUE : VK_FALSE,
        .compareOp        = VulkanUtils::PathfinderCompareOpToVulkan(samplerSpec.CompareOp),
        .minLod           = samplerSpec.MinLod,
        .maxLod           = IsNearlyEqual(samplerSpec.MaxLod, .0f) ? VK_LOD_CLAMP_NONE : samplerSpec.MaxLod,
        .borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // VK_BORDER_COLOR_INT_OPAQUE_BLACK;  // TODO: configurable??
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(device->GetLogicalDevice(), &samplerCI, nullptr, &sampler), "Failed to create vulkan sampler!");
    return sampler;
}

void VulkanSamplerStorage::DestroySamplerImpl(void* sampler)
{
    PFR_ASSERT(sampler, "Not valid sampler to destroy!");
    vkDestroySampler(VulkanContext::Get().GetDevice()->GetLogicalDevice(), (VkSampler)sampler, nullptr);
}

}  // namespace Pathfinder