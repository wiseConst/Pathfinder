#include "PathfinderPCH.h"
#include "VulkanSwapchain.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "Renderer/Renderer.h"

#include <GLFW/glfw3.h>
#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace Pathfinder
{

struct SwapchainSupportDetails final
{
  public:
    static SwapchainSupportDetails QuerySwapchainSupportDetails(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface)
    {
        SwapchainSupportDetails Details = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &Details.SurfaceCapabilities),
                 "Failed to query surface capabilities.");

        uint32_t surfaceFormatNum{0};
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatNum, nullptr),
                 "Failed to query number of surface formats.");
        PFR_ASSERT(surfaceFormatNum > 0, "Surface format count is not valid!");

        Details.ImageFormats.resize(surfaceFormatNum);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatNum, Details.ImageFormats.data()),
                 "Failed to query surface formats.");

        uint32_t presentModeCount{0};
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr),
                 "Failed to query number of present modes.");
        PFR_ASSERT(presentModeCount > 0, "Present mode count is not valid!");

        Details.PresentModes.resize(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, Details.PresentModes.data()),
                 "Failed to query present modes.");

        return Details;
    }

    VkSurfaceFormatKHR ChooseBestSurfaceFormat() const
    {
        // If Vulkan returned an unknown format, then just force what we want.
        if (ImageFormats.size() == 1 && ImageFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            VkSurfaceFormatKHR format = {};
            // ImGui uses VK_FORMAT_B8G8R8A8_UNORM
            format.format     = VK_FORMAT_B8G8R8A8_UNORM;  // VK_FORMAT_B8G8R8A8_SRGB;
            format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            return format;
        }

        for (const auto& imageFormat : ImageFormats)
        {
            // ImGui uses VK_FORMAT_B8G8R8A8_UNORM
            if (imageFormat.format == VK_FORMAT_B8G8R8A8_UNORM /*VK_FORMAT_B8G8R8A8_SRGB*/ &&
                imageFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return imageFormat;
            }
        }

        LOG_WARN("Failed to choose best swapchain surface format.");
        return ImageFormats[0];
    }

    VkPresentModeKHR ChooseBestPresentMode(bool bIsVSync) const
    {
        if (bIsVSync) return VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& presentMode : PresentModes)
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) return presentMode;  // Battery-drain mode on mobile devices

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ChooseBestExtent(GLFWwindow* window) const
    {
        if (SurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return SurfaceCapabilities.currentExtent;

        int32_t width{0}, height{0};
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        actualExtent.width =
            std::clamp(actualExtent.width, SurfaceCapabilities.minImageExtent.width, SurfaceCapabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, SurfaceCapabilities.minImageExtent.height, SurfaceCapabilities.maxImageExtent.height);

        return actualExtent;
    }

  public:
    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    std::vector<VkPresentModeKHR> PresentModes;
    std::vector<VkSurfaceFormatKHR> ImageFormats;
};

VulkanSwapchain::VulkanSwapchain(void* windowHandle) noexcept : m_WindowHandle(windowHandle)
{
    VK_CHECK(glfwCreateWindowSurface(VulkanContext::Get().GetInstance(), static_cast<GLFWwindow*>(m_WindowHandle), nullptr, &m_Surface),
             "Failed to create surface!");

    Invalidate();

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    m_SurfaceFullScreenExclusiveWin32Info.pNext = nullptr;
    m_SurfaceFullScreenExclusiveWin32Info.hmonitor =
        MonitorFromWindow(glfwGetWin32Window(static_cast<GLFWwindow*>(m_WindowHandle)), MONITOR_DEFAULTTONEAREST);

    m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;
    m_SurfaceFullScreenExclusiveInfo.pNext               = &m_SurfaceFullScreenExclusiveWin32Info;
#endif
}

void VulkanSwapchain::SetClearColor(const glm::vec3& clearColor)
{
    const auto& rendererData = Renderer::GetRendererData();

    const auto& commandBuffer = rendererData->CurrentRenderCommandBuffer.lock();
    PFR_ASSERT(commandBuffer, "Failed to retrieve current render command buffer!");

    const auto& vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    vulkanCommandBuffer->BeginDebugLabel("SwapchainClearColor", clearColor);
    {
        const auto imageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 1, 0, 1, 0);
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);

        const VkClearColorValue clearColorValue = {clearColor.x, clearColor.y, clearColor.z, 1.0f};
        constexpr VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto rawCommandBuffer             = static_cast<VkCommandBuffer>(commandBuffer->Get());
        vkCmdClearColorImage(rawCommandBuffer, m_Images[m_ImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColorValue, 1, &range);
    }

    {
        const auto imageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1, 0, 1, 0);
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);
    }

    vulkanCommandBuffer->EndDebugLabel();
}

void VulkanSwapchain::SetWindowMode(const EWindowMode windowMode)
{
    if (windowMode == m_WindowMode) return;

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    if (m_WindowMode == EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
    {
        VK_CHECK(vkReleaseFullScreenExclusiveModeEXT(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle),
                 "Failed to release full screen exclusive mode!");
    }

    if (windowMode == EWindowMode::WINDOW_MODE_WINDOWED)
    {
        m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
    }
    else if (windowMode == EWindowMode::WINDOW_MODE_BORDERLESS_FULLSCREEN)
    {
        m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
    }
    else if (windowMode == EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
    {
        m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
    }
#endif

    m_WindowMode = windowMode;
    Invalidate();

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    if (m_WindowMode == EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
    {
        VK_CHECK(vkAcquireFullScreenExclusiveModeEXT(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle),
                 "Failed to acquire full screen exclusive mode!");
    }
#endif
}

void VulkanSwapchain::Invalidate()
{
    auto& context              = VulkanContext::Get();
    const auto& logicalDevice  = context.GetDevice()->GetLogicalDevice();
    const auto& physicalDevice = context.GetDevice()->GetPhysicalDevice();

    const auto oldSwapchain = m_Handle;
    if (oldSwapchain)
    {
        std::ranges::for_each(m_ImageAcquiredSemaphores, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
        std::ranges::for_each(m_ImageViews, [&](auto& imageView) { vkDestroyImageView(logicalDevice, imageView, nullptr); });
    }

    std::ranges::for_each(
        m_ImageAcquiredSemaphores,
        [&](auto& semaphore)
        {
            constexpr VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            VK_CHECK(vkCreateSemaphore(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &semaphoreCreateInfo, nullptr, &semaphore),
                     "Failed to create semaphore!");
        });

    m_ImageIndex = 0;
    m_FrameIndex = 0;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainCreateInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped                  = VK_TRUE;
    swapchainCreateInfo.imageArrayLayers         = 1;
    swapchainCreateInfo.surface                  = m_Surface;
    swapchainCreateInfo.oldSwapchain             = oldSwapchain;

    const auto Details               = SwapchainSupportDetails::QuerySwapchainSupportDetails(physicalDevice, m_Surface);
    swapchainCreateInfo.preTransform = Details.SurfaceCapabilities.currentTransform;

    m_ImageFormat                       = Details.ChooseBestSurfaceFormat();
    swapchainCreateInfo.imageColorSpace = m_ImageFormat.colorSpace;
    swapchainCreateInfo.imageFormat     = m_ImageFormat.format;

    m_CurrentPresentMode            = Details.ChooseBestPresentMode(m_bVSync);
    swapchainCreateInfo.presentMode = m_CurrentPresentMode;

    PFR_ASSERT(Details.SurfaceCapabilities.maxImageCount > 0, "Swapchain max image count less than zero!");
    m_ImageCount                      = std::clamp(Details.SurfaceCapabilities.minImageCount + 1, Details.SurfaceCapabilities.minImageCount,
                                                   Details.SurfaceCapabilities.maxImageCount);
    swapchainCreateInfo.minImageCount = m_ImageCount;

#if VK_EXCLUSIVE_FULL_SCREEN_TEST
    if (m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
        m_ImageExtent = Details.SurfaceCapabilities.maxImageExtent;
    else
#endif
        m_ImageExtent = Details.ChooseBestExtent(static_cast<GLFWwindow*>(m_WindowHandle));

    swapchainCreateInfo.imageExtent = m_ImageExtent;

    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (Details.SurfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (context.GetDevice()->GetQueueFamilyIndices().GraphicsFamily != context.GetDevice()->GetQueueFamilyIndices().PresentFamily)
    {
        const uint32_t indices[]                  = {context.GetDevice()->GetQueueFamilyIndices().GraphicsFamily,
                                                     context.GetDevice()->GetQueueFamilyIndices().PresentFamily};
        swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices   = indices;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    swapchainCreateInfo.pNext = &m_SurfaceFullScreenExclusiveInfo;
#endif

    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, nullptr, &m_Handle), "Failed to create vulkan swapchain!");
    if (oldSwapchain) vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);

    VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, m_Handle, &m_ImageCount, nullptr), "Failed to retrieve swapchain images !");
    PFR_ASSERT(m_ImageCount > 0, "Swapchain image can't be less than zero!");

    m_Images.resize(m_ImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, m_Handle, &m_ImageCount, m_Images.data()), "Failed to acquire swapchain images!");

    m_ImageViews.resize(m_Images.size());
    for (uint32_t i = 0; i < m_ImageViews.size(); ++i)
    {
        ImageUtils::CreateImageView(m_Images[i], m_ImageViews[i], m_ImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
                                    1);

        std::string debugName = "Swapchain image[" + std::to_string(i) + "]";
        VK_SetDebugName(logicalDevice, &m_Images[i], VK_OBJECT_TYPE_IMAGE, debugName.data());
        debugName = "Swapchain image view[" + std::to_string(i) + "]";
        VK_SetDebugName(logicalDevice, &m_ImageViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, debugName.data());
    }

    m_ImageLayouts.resize(m_Images.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    m_bWasInvalidated = true;
}

void VulkanSwapchain::AcquireImage()
{
    m_bWasInvalidated = false;

    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_UNDEFINED;

    const auto result =
        vkAcquireNextImageKHR(logicalDevice, m_Handle, UINT64_MAX, m_ImageAcquiredSemaphores[m_FrameIndex], VK_NULL_HANDLE, &m_ImageIndex);
    if (result == VK_SUCCESS) return;

    if (result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        const auto ResultMessage = std::string("Failed to acquire image from the swapchain! Result is: ") + VK_GetResultString(result);
        PFR_ASSERT(false, ResultMessage.data());
    }

    Recreate();
}

void VulkanSwapchain::PresentImage()
{
    m_bWasInvalidated = false;

    if (m_ImageLayouts[m_ImageIndex] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS);
        vulkanCommandBuffer->BeginRecording(true);

        const auto srcImageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex],
                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 0, 1, 0, 1, 0);

        // NOTE: TBH idk which stages to use here
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &srcImageBarrier);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit();
    }

    VkPresentInfoKHR presentInfo   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.pImageIndices      = &m_ImageIndex;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_Handle;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_ImageAcquiredSemaphores[m_FrameIndex];

    const auto imagePresentBegin                = Timer::Now();
    const auto result                           = vkQueuePresentKHR(VulkanContext::Get().GetDevice()->GetPresentQueue(), &presentInfo);
    const std::chrono::duration<float> duration = Timer::Now() - imagePresentBegin;
    // LOG_WARN("Swapchain image[%u] present took: %0.2fms", m_ImageIndex, duration.count() * 1000.0f);
    // Renderer::GetStats().PresentTime = imagePresentEnd - imagePresentBegin;

    if (result == VK_SUCCESS)
    {
        m_FrameIndex = (m_FrameIndex + 1) % s_FRAMES_IN_FLIGHT;
        return;
    }

    Recreate();
}

void VulkanSwapchain::Recreate()
{
    if (Application::Get().GetWindow()->IsMinimized()) return;
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    const auto recreationBegin = Timer::Now();
    Invalidate();
    const auto duration = std::chrono::duration<float>(Timer::Now() - recreationBegin);

#if PFR_DEBUG
    LOG_TAG_TRACE(VULKAN, "Swapchain recreated with: (%u, %u). Took: %0.2fms", m_ImageExtent.width, m_ImageExtent.height,
                  duration.count() * 1000.0f);
#endif

    for (auto& resizeCallback : m_ResizeCallbacks)
    {
        resizeCallback(m_ImageExtent.width, m_ImageExtent.height);
    }
}

void VulkanSwapchain::Destroy()
{
    auto& context = VulkanContext::Get();
    context.GetDevice()->WaitDeviceOnFinish();

    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    vkDestroySwapchainKHR(logicalDevice, m_Handle, nullptr);
    vkDestroySurfaceKHR(context.GetInstance(), m_Surface, nullptr);

    std::ranges::for_each(m_ImageViews,
                          [&](auto& imageView) { vkDestroyImageView(context.GetDevice()->GetLogicalDevice(), imageView, nullptr); });
    std::ranges::for_each(m_ImageAcquiredSemaphores, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
}

void VulkanSwapchain::CopyToSwapchain(const Shared<Image>& image)
{
    auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS);
    vulkanCommandBuffer->BeginRecording(true);
    vulkanCommandBuffer->BeginDebugLabel("CopyToSwapchain", glm::vec3(0.9f, 0.1f, 0.1f));

    {
        const auto srcImageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, 1, 0, 1, 0);
        const auto dstImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            (VkImage)image->Get(),
            ImageUtils::IsDepthFormat(image->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            ImageUtils::PathfinderImageLayoutToVulkan(image->GetSpecification().Layout), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0,
            VK_ACCESS_TRANSFER_READ_BIT, 1, 0, 1, 0);

        const std::vector<VkImageMemoryBarrier> imageBarriers = {srcImageBarrier, dstImageBarrier};
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
                                           0, nullptr, 0, nullptr, static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
    }

    VkImageBlit region               = {};
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.aspectMask =
        ImageUtils::IsDepthFormat(image->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcOffsets[1] =
        VkOffset3D{static_cast<int32_t>(image->GetSpecification().Width), static_cast<int32_t>(image->GetSpecification().Height), 1};

    region.dstSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstOffsets[1]             = VkOffset3D{static_cast<int32_t>(m_ImageExtent.width), static_cast<int32_t>(m_ImageExtent.height), 1};

    vulkanCommandBuffer->BlitImage((VkImage)image->Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Images[m_ImageIndex],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

    {
        const auto srcImageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, 0, 1, 0, 1, 0);
        m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        const auto dstImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            (VkImage)image->Get(),
            ImageUtils::IsDepthFormat(image->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ImageUtils::PathfinderImageLayoutToVulkan(image->GetSpecification().Layout),
            VK_ACCESS_TRANSFER_READ_BIT, 0, 1, 0, 1, 0);

        const std::vector<VkImageMemoryBarrier> imageBarriers = {srcImageBarrier, dstImageBarrier};
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, static_cast<uint32_t>(imageBarriers.size()),
                                           imageBarriers.data());
    }

    vulkanCommandBuffer->EndDebugLabel();
    vulkanCommandBuffer->EndRecording();
    vulkanCommandBuffer->Submit();
}

}  // namespace Pathfinder