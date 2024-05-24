#include <PathfinderPCH.h>
#include "VulkanSwapchain.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Renderer.h>

#include <GLFW/glfw3.h>
#if PFR_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace Pathfinder
{

static VkPresentModeKHR PathfinderPresentModeToVulkan(const EPresentMode presentMode)
{
    switch (presentMode)
    {
        case EPresentMode::PRESENT_MODE_FIFO: return VK_PRESENT_MODE_FIFO_KHR;
        case EPresentMode::PRESENT_MODE_IMMEDIATE: return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case EPresentMode::PRESENT_MODE_MAILBOX: return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static EPresentMode VulkanPresentModeToPathfinder(const VkPresentModeKHR presentMode)
{
    switch (presentMode)
    {
        case VK_PRESENT_MODE_FIFO_KHR: return EPresentMode::PRESENT_MODE_FIFO;
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return EPresentMode::PRESENT_MODE_IMMEDIATE;
        case VK_PRESENT_MODE_MAILBOX_KHR: return EPresentMode::PRESENT_MODE_MAILBOX;
    }

    return EPresentMode::PRESENT_MODE_FIFO;
}

static EImageFormat VulkanImageFormatToPathfinder(const VkFormat imageFormat)
{
    switch (imageFormat)
    {
        case VK_FORMAT_UNDEFINED: return EImageFormat::FORMAT_UNDEFINED;
        case VK_FORMAT_R8G8B8_UNORM: return EImageFormat::FORMAT_RGB8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM: return EImageFormat::FORMAT_RGBA8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM: return EImageFormat::FORMAT_BGRA8_UNORM;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32;
        case VK_FORMAT_R8_UNORM: return EImageFormat::FORMAT_R8_UNORM;
        case VK_FORMAT_R16_UNORM: return EImageFormat::FORMAT_R16_UNORM;
        case VK_FORMAT_R16_SFLOAT: return EImageFormat::FORMAT_R16F;
        case VK_FORMAT_R32_SFLOAT: return EImageFormat::FORMAT_R32F;
        case VK_FORMAT_R64_SFLOAT: return EImageFormat::FORMAT_R64F;
        case VK_FORMAT_R16G16B16_UNORM: return EImageFormat::FORMAT_RGB16_UNORM;
        case VK_FORMAT_R16G16B16_SFLOAT: return EImageFormat::FORMAT_RGB16F;
        case VK_FORMAT_R16G16B16A16_UNORM: return EImageFormat::FORMAT_RGBA16_UNORM;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return EImageFormat::FORMAT_RGBA16F;
        case VK_FORMAT_R32G32B32_SFLOAT: return EImageFormat::FORMAT_RGB32F;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return EImageFormat::FORMAT_RGBA32F;
        case VK_FORMAT_R64G64B64_SFLOAT: return EImageFormat::FORMAT_RGB64F;
        case VK_FORMAT_R64G64B64A64_SFLOAT: return EImageFormat::FORMAT_RGBA64F;

        case VK_FORMAT_D16_UNORM: return EImageFormat::FORMAT_D16_UNORM;
        case VK_FORMAT_D32_SFLOAT: return EImageFormat::FORMAT_D32F;
        case VK_FORMAT_S8_UINT: return EImageFormat::FORMAT_S8_UINT;
        case VK_FORMAT_D16_UNORM_S8_UINT: return EImageFormat::FORMAT_D16_UNORM_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT: return EImageFormat::FORMAT_D24_UNORM_S8_UINT;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return EImageFormat::FORMAT_UNDEFINED;
}

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

    VkPresentModeKHR ChooseBestPresentMode(const VkPresentModeKHR requestedPresentMode) const
    {
        if (requestedPresentMode == VK_PRESENT_MODE_FIFO_KHR) return VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& presentMode : PresentModes)
            if (presentMode == requestedPresentMode) return presentMode;

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

#if PFR_WINDOWS
    m_SurfaceFullScreenExclusiveWin32Info.pNext = nullptr;
    m_SurfaceFullScreenExclusiveWin32Info.hmonitor =
        MonitorFromWindow(glfwGetWin32Window(static_cast<GLFWwindow*>(m_WindowHandle)), MONITOR_DEFAULTTONEAREST);
#endif

    m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;
    m_SurfaceFullScreenExclusiveInfo.pNext               = &m_SurfaceFullScreenExclusiveWin32Info;

    Invalidate();
}

NODISCARD const EImageFormat VulkanSwapchain::GetImageFormat() const
{
    return VulkanImageFormatToPathfinder(m_ImageFormat.format);
}

void VulkanSwapchain::SetClearColor(const glm::vec3& clearColor)
{
    const auto& rd = Renderer::GetRendererData();

    const auto& commandBuffer = rd->CurrentRenderCommandBuffer.lock();
    PFR_ASSERT(commandBuffer, "Failed to retrieve current render command buffer!");

    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    vulkanCommandBuffer->BeginDebugLabel("SwapchainClearColor", clearColor);

    {
        if (m_ImageLayouts[m_ImageIndex] != VK_IMAGE_LAYOUT_GENERAL)
        {
            const auto imageBarrier =
                VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex],
                                                     VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE,
                                                     VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, 1, 0, 1, 0);
            m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_GENERAL;

            vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                               VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        }

        const VkClearColorValue clearColorValue = {clearColor.x, clearColor.y, clearColor.z, 1.0f};
        constexpr VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto rawCommandBuffer             = static_cast<VkCommandBuffer>(commandBuffer->Get());

        vkCmdClearColorImage(rawCommandBuffer, m_Images[m_ImageIndex], m_ImageLayouts[m_ImageIndex], &clearColorValue, 1, &range);
    }

    {
        const auto dstPipelineStages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        const auto imageBarrier      = VulkanUtility::GetImageMemoryBarrier(
            m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, dstPipelineStages, VK_ACCESS_2_NONE, 1, 0, 1, 0);
        m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_TRANSFER_BIT, dstPipelineStages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
                                           nullptr, 1, &imageBarrier);
    }

    vulkanCommandBuffer->EndDebugLabel();
}

void VulkanSwapchain::SetWindowMode(const EWindowMode windowMode)
{
    if (windowMode == m_WindowMode) return;

    if (m_WindowMode == EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
    {
        VK_CHECK(vkReleaseFullScreenExclusiveModeEXT(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle),
                 "Failed to release fullscreen exclusive mode!");
    }

    switch (windowMode)
    {
        case EWindowMode::WINDOW_MODE_WINDOWED:
            m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
            break;
        case EWindowMode::WINDOW_MODE_BORDERLESS_FULLSCREEN:
            m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
            break;
        case EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE:
            m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
            break;
    }

    m_LastWindowMode = m_WindowMode;
    m_WindowMode     = windowMode;
    m_bNeedsRecreate = true;
}

void VulkanSwapchain::Invalidate()
{
    auto& context              = VulkanContext::Get();
    const auto& logicalDevice  = context.GetDevice()->GetLogicalDevice();
    const auto& physicalDevice = context.GetDevice()->GetPhysicalDevice();

    m_bNeedsRecreate = false;

    auto oldSwapchain = m_Handle;
    if (oldSwapchain)
    {
        std::ranges::for_each(m_ImageViews,
                              [&](auto& imageView)
                              {
                                  --Renderer::GetStats().ImageViewCount;
                                  vkDestroyImageView(logicalDevice, imageView, nullptr);
                              });
        std::ranges::for_each(m_RenderSemaphore, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
        std::ranges::for_each(m_ImageAcquiredSemaphore, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
        std::ranges::for_each(m_RenderFence, [&](auto& fence) { vkDestroyFence(logicalDevice, fence, nullptr); });
    }

    std::ranges::for_each(m_ImageAcquiredSemaphore,
                          [&](auto& semaphore)
                          {
                              constexpr VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                              VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore),
                                       "Failed to create semaphore!");

                              const std::string semaphoreName = "VK_IMAGE_ACQUIRED_SEMAPHORE";
                              VK_SetDebugName(logicalDevice, semaphore, VK_OBJECT_TYPE_SEMAPHORE, semaphoreName.data());
                          });

    std::ranges::for_each(m_RenderSemaphore,
                          [&](auto& semaphore)
                          {
                              constexpr VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                              VK_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &semaphore),
                                       "Failed to create semaphore!");

                              const std::string semaphoreName = "VK_RENDER_FINISHED_SEMAPHORE";
                              VK_SetDebugName(logicalDevice, semaphore, VK_OBJECT_TYPE_SEMAPHORE, semaphoreName.data());
                          });

    std::ranges::for_each(
        m_RenderFence,
        [&](auto& fence)
        {
            constexpr VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
            VK_CHECK(vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &fence), "Failed to create fence!");

            const std::string fenceName = "VK_RENDER_FINISHED_FENCE";
            VK_SetDebugName(logicalDevice, fence, VK_OBJECT_TYPE_FENCE, fenceName.data());
        });

    m_ImageIndex = 0;
    m_FrameIndex = 0;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainCreateInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.clipped                  = VK_TRUE;
    swapchainCreateInfo.imageArrayLayers         = 1;
    swapchainCreateInfo.surface                  = m_Surface;

    if (m_CurrentPresentMode == VK_PRESENT_MODE_FIFO_KHR &&
        m_PresentMode != EPresentMode::PRESENT_MODE_FIFO)  // Current present mode is not fifo, but we want to make it fifo.
    {
        vkDestroySwapchainKHR(logicalDevice, m_Handle, nullptr);
        oldSwapchain = nullptr;
    }
    else
    {
        swapchainCreateInfo.oldSwapchain = oldSwapchain;
    }

    const auto Details               = SwapchainSupportDetails::QuerySwapchainSupportDetails(physicalDevice, m_Surface);
    swapchainCreateInfo.preTransform = Details.SurfaceCapabilities.currentTransform;

    m_ImageFormat                       = Details.ChooseBestSurfaceFormat();
    swapchainCreateInfo.imageColorSpace = m_ImageFormat.colorSpace;
    swapchainCreateInfo.imageFormat     = m_ImageFormat.format;

    m_CurrentPresentMode            = Details.ChooseBestPresentMode(PathfinderPresentModeToVulkan(m_PresentMode));
    swapchainCreateInfo.presentMode = m_CurrentPresentMode;
    m_PresentMode                   = VulkanPresentModeToPathfinder(m_CurrentPresentMode);

#if PFR_DEBUG
    const char* presentModeStr = m_PresentMode == EPresentMode::PRESENT_MODE_FIFO        ? "FIFO"
                                 : m_PresentMode == EPresentMode::PRESENT_MODE_IMMEDIATE ? "IMMEDIATE"
                                 : m_PresentMode == EPresentMode::PRESENT_MODE_MAILBOX   ? "MAILBOX"
                                                                                         : "UNKNOWN";
    LOG_DEBUG("PresentMode: {}.", presentModeStr);
#endif

    PFR_ASSERT(Details.SurfaceCapabilities.maxImageCount > 0, "Swapchain max image count less than zero!");
    uint32_t imageCount               = std::clamp(Details.SurfaceCapabilities.minImageCount + 1, Details.SurfaceCapabilities.minImageCount,
                                                   Details.SurfaceCapabilities.maxImageCount);
    swapchainCreateInfo.minImageCount = imageCount;

    if (m_SurfaceFullScreenExclusiveInfo.fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
        m_ImageExtent = Details.SurfaceCapabilities.maxImageExtent;
    else
        m_ImageExtent = Details.ChooseBestExtent(static_cast<GLFWwindow*>(m_WindowHandle));

    swapchainCreateInfo.imageExtent = m_ImageExtent;

    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (Details.SurfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (context.GetDevice()->GetGraphicsFamily() != context.GetDevice()->GetPresentFamily())
    {
        PFR_ASSERT(false, "This shouldn't happen: GraphicsFamily != PresentFamily!");
        const uint32_t indices[]                  = {context.GetDevice()->GetGraphicsFamily(), context.GetDevice()->GetPresentFamily()};
        swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices   = indices;
    }
    else
    {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // Literally will be called only when constructing swapchain.
    if (!m_Handle)
    {
        glfwGetWindowPos((GLFWwindow*)m_WindowHandle, &m_LastPosX, &m_LastPosY);

        m_LastWidth  = m_ImageExtent.width;
        m_LastHeight = m_ImageExtent.height;
    }

    swapchainCreateInfo.pNext = &m_SurfaceFullScreenExclusiveInfo;
    VK_CHECK(vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, nullptr, &m_Handle), "Failed to create vulkan swapchain!");
    if (oldSwapchain) vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);

    VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, m_Handle, &imageCount, nullptr), "Failed to retrieve swapchain images !");
    PFR_ASSERT(imageCount > 0, "Swapchain image can't be less than zero!");

    m_Images.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(logicalDevice, m_Handle, &imageCount, m_Images.data()), "Failed to acquire swapchain images!");

    m_ImageViews.resize(m_Images.size());
    for (uint32_t i = 0; i < m_ImageViews.size(); ++i)
    {
        ImageUtils::CreateImageView(m_Images[i], m_ImageViews[i], m_ImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1,
                                    1);

        std::string debugName = "Swapchain image[" + std::to_string(i) + "]";
        VK_SetDebugName(logicalDevice, m_Images[i], VK_OBJECT_TYPE_IMAGE, debugName.data());
        debugName = "Swapchain image view[" + std::to_string(i) + "]";
        VK_SetDebugName(logicalDevice, m_ImageViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, debugName.data());
    }

    Renderer::GetStats().ImageViewCount += m_ImageViews.size();
    m_ImageLayouts.assign(m_Images.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    switch (m_WindowMode)
    {
        case EWindowMode::WINDOW_MODE_WINDOWED:
        {
            if (m_WindowMode != m_LastWindowMode)
            {
                glfwSetWindowMonitor((GLFWwindow*)m_WindowHandle, NULL, m_LastPosX, m_LastPosY, m_LastWidth, m_LastHeight, GLFW_DONT_CARE);
            }
            break;
        }
        case EWindowMode::WINDOW_MODE_BORDERLESS_FULLSCREEN:
        {
            if (m_LastWindowMode != EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
            {
                glfwGetWindowPos((GLFWwindow*)m_WindowHandle, &m_LastPosX, &m_LastPosY);
                glfwGetWindowSize((GLFWwindow*)m_WindowHandle, &m_LastWidth, &m_LastHeight);
            }
            break;
        }
        case EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE:
        {
            if (m_WindowMode != m_LastWindowMode)
            {
                glfwGetWindowPos((GLFWwindow*)m_WindowHandle, &m_LastPosX, &m_LastPosY);
                const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
                PFR_ASSERT(videoMode, "Invalid video mode!");

                glfwSetWindowMonitor((GLFWwindow*)m_WindowHandle, glfwGetPrimaryMonitor(), 0, 0, videoMode->width, videoMode->height,
                                     videoMode->refreshRate);
            }
            break;
        }
    }

    if (m_WindowMode == EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE)
    {
        VK_CHECK(vkAcquireFullScreenExclusiveModeEXT(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle),
                 "Failed to acquire full screen exclusive mode!");
    }

    m_LastWindowMode = m_WindowMode;
}

bool VulkanSwapchain::AcquireImage()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    VK_CHECK(vkWaitForFences(logicalDevice, 1, &m_RenderFence[m_FrameIndex], VK_TRUE, UINT64_MAX),
             "Failed to wait on swapchain-render submit fence!");

    const auto result =
        vkAcquireNextImageKHR(logicalDevice, m_Handle, UINT64_MAX, m_ImageAcquiredSemaphore[m_FrameIndex], VK_NULL_HANDLE, &m_ImageIndex);
    m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_UNDEFINED;

    if (result == VK_SUCCESS)
    {
        VK_CHECK(vkResetFences(logicalDevice, 1, &m_RenderFence[m_FrameIndex]), "Failed to reset swapchain-render submit fence!");
        return true;
    }

    if (result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        const auto ResultMessage = std::string("Failed to acquire image from the swapchain! Result is: ") + VK_GetResultString(result);
        PFR_ASSERT(false, ResultMessage.data());
    }

    m_bNeedsRecreate = true;
    return false;
}

void VulkanSwapchain::PresentImage()
{
    bool bWasEverUsed = true;  // In case Renderer didn't use it, we should set waitSemaphore to imageAvaliable, otherwise renderSemaphore.
    if (m_ImageLayouts[m_ImageIndex] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        const CommandBufferSpecification cbSpec = {ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS,
                                                   ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint8_t>(m_FrameIndex),
                                                   ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        auto vulkanCommandBuffer                = MakeShared<VulkanCommandBuffer>(cbSpec);
        vulkanCommandBuffer->BeginRecording(true);

        const auto imageBarrier =
            VulkanUtility::GetImageMemoryBarrier(m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex],
                                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                 VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, 1, 0, 1, 0);

        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit()->Wait();

        bWasEverUsed = false;
    }

    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.pImageIndices    = &m_ImageIndex;
    presentInfo.swapchainCount   = 1;
    presentInfo.pSwapchains      = &m_Handle;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = bWasEverUsed ? &m_RenderSemaphore[m_FrameIndex] : &m_ImageAcquiredSemaphore[m_FrameIndex];

    Timer t                                   = {};
    const auto result                         = vkQueuePresentKHR(VulkanContext::Get().GetDevice()->GetPresentQueue(), &presentInfo);
    Renderer::GetStats().SwapchainPresentTime = t.GetElapsedMilliseconds();

    if (result == VK_SUCCESS)
    {
        m_FrameIndex = (m_FrameIndex + 1) % s_FRAMES_IN_FLIGHT;
    }
    else
        m_bNeedsRecreate = true;

    if (m_bNeedsRecreate) Recreate();
}

void VulkanSwapchain::BeginPass(const Shared<CommandBuffer>& commandBuffer, const bool bPreserveContents)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    commandBuffer->BeginDebugLabel("SwapchainPass");

    const auto srcPipelineStages = bPreserveContents ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT |
                                                           VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                                     : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    const auto imageBarrier      = VulkanUtility::GetImageMemoryBarrier(
        m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        srcPipelineStages, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        bPreserveContents ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                               : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        1, 0, 1, 0);

    vulkanCommandBuffer->InsertBarrier(srcPipelineStages, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                                       nullptr, 0, nullptr, 1, &imageBarrier);

    m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo attachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attachmentInfo.imageView                 = m_ImageViews[m_ImageIndex];
    attachmentInfo.loadOp                    = bPreserveContents ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.imageLayout               = m_ImageLayouts[m_ImageIndex];

    VkRenderingInfo renderingInfo      = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.layerCount           = 1;
    renderingInfo.pColorAttachments    = &attachmentInfo;
    renderingInfo.renderArea           = {{0, 0}, {m_ImageExtent}};

    vulkanCommandBuffer->BeginRendering(&renderingInfo);
}

void VulkanSwapchain::EndPass(const Shared<CommandBuffer>& commandBuffer)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    vulkanCommandBuffer->EndRendering();

    const auto srcPipelineStages =
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto imageBarrier = VulkanUtility::GetImageMemoryBarrier(
        m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, m_ImageLayouts[m_ImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, srcPipelineStages,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, 1, 0, 1, 0);

    vulkanCommandBuffer->InsertBarrier(srcPipelineStages,
                                       VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    commandBuffer->EndDebugLabel();
}

void VulkanSwapchain::Recreate()
{
    if (Application::Get().GetWindow()->IsMinimized()) return;
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

#if PFR_DEBUG
    Timer t = {};
#endif

    Invalidate();

#if PFR_DEBUG
    LOG_TRACE("Swapchain recreated with: ({}, {}). Took: {:.2f}ms", m_ImageExtent.width, m_ImageExtent.height, t.GetElapsedMilliseconds());
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
                          [&](auto& imageView)
                          {
                              --Renderer::GetStats().ImageViewCount;
                              vkDestroyImageView(logicalDevice, imageView, nullptr);
                          });
    std::ranges::for_each(m_ImageAcquiredSemaphore, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
    std::ranges::for_each(m_RenderSemaphore, [&](auto& semaphore) { vkDestroySemaphore(logicalDevice, semaphore, nullptr); });
    std::ranges::for_each(m_RenderFence, [&](auto& fence) { vkDestroyFence(logicalDevice, fence, nullptr); });
}

void VulkanSwapchain::CopyToSwapchain(const Shared<Image>& image)
{
    Shared<VulkanCommandBuffer> vulkanCommandBuffer = nullptr;

    bool bFromRenderer = false;
    const auto& rd     = Renderer::GetRendererData();
    if (const auto& commandBuffer = rd->CurrentRenderCommandBuffer.lock())
    {
        vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
        PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

        bFromRenderer = true;
    }
    else
    {
        const CommandBufferSpecification cbSpec = {ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS,
                                                   ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint8_t>(m_FrameIndex),
                                                   ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        vulkanCommandBuffer                     = MakeShared<VulkanCommandBuffer>(cbSpec);
        vulkanCommandBuffer->BeginRecording(true);
    }

    vulkanCommandBuffer->BeginDebugLabel("CopyToSwapchain", glm::vec3(0.9f, 0.1f, 0.1f));

    {
        const auto srcImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            (VkImage)image->Get(),
            ImageUtils::IsDepthFormat(image->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            ImageUtils::PathfinderImageLayoutToVulkan(image->GetSpecification().Layout), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
            1, 0, 1, 0);
        const auto dstImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            1, 0, 1, 0);

        const std::vector<VkImageMemoryBarrier2> imageBarriers = {srcImageBarrier, dstImageBarrier};
        vulkanCommandBuffer->InsertBarrier(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, static_cast<uint32_t>(imageBarriers.size()),
                                           imageBarriers.data());
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
        const auto dstImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            m_Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_NONE, 1, 0, 1, 0);
        m_ImageLayouts[m_ImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        const auto srcImageBarrier = VulkanUtility::GetImageMemoryBarrier(
            (VkImage)image->Get(),
            ImageUtils::IsDepthFormat(image->GetSpecification().Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ImageUtils::PathfinderImageLayoutToVulkan(image->GetSpecification().Layout),
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_NONE, 1, 0, 1, 0);

        const std::vector<VkImageMemoryBarrier2> imageBarriers = {srcImageBarrier, dstImageBarrier};
        vulkanCommandBuffer->InsertBarrier(
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());
    }

    vulkanCommandBuffer->EndDebugLabel();

    if (!bFromRenderer)
    {
        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit()->Wait();
    }
}

}  // namespace Pathfinder