#ifndef VULKANSWAPCHAIN_H
#define VULKANSWAPCHAIN_H

#include "Renderer/Swapchain.h"
#include "VulkanCore.h"

namespace Pathfinder
{

#define VK_EXCLUSIVE_FULL_SCREEN_TEST 0

class VulkanCommandBuffer;

class VulkanSwapchain final : public Swapchain
{
  public:
    VulkanSwapchain(void* windowHandle) noexcept;
    virtual ~VulkanSwapchain() override { Destroy(); }

    NODISCARD FORCEINLINE const uint32_t GetCurrentFrameIndex() const { return m_FrameIndex; }

    void SetClearColor(const glm::vec3& clearColor) final override;
    void SetVSync(bool bVSync) final override
    {
        if (m_bVSync == bVSync) return;

        m_bVSync = bVSync;
        Recreate();
        LOG_WARN("VSync: %s.", m_bVSync ? "ON" : "OFF");
    }
    void SetWindowMode(const EWindowMode windowMode) final override;

    void Invalidate() final override;

  private:
    VkSwapchainKHR m_Handle  = VK_NULL_HANDLE;
    void* m_WindowHandle     = nullptr;
    EWindowMode m_WindowMode = EWindowMode::WINDOW_MODE_WINDOWED;
    VkSurfaceKHR m_Surface   = VK_NULL_HANDLE;
    bool m_bVSync            = false;
    VulkanSempahorePerFrame m_ImageAcquiredSemaphores;

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    VkSurfaceFullScreenExclusiveWin32InfoEXT m_SurfaceFullScreenExclusiveWin32Info = {
        VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT};
    VkSurfaceFullScreenExclusiveInfoEXT m_SurfaceFullScreenExclusiveInfo = {VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT};
#endif

    uint32_t m_ImageCount                 = 0;
    VkPresentModeKHR m_CurrentPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkSurfaceFormatKHR m_ImageFormat      = {VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
    VkExtent2D m_ImageExtent              = {1280, 720};

    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;

    uint32_t m_ImageIndex{0};
    uint32_t m_FrameIndex{0};

    void Recreate();
    void Destroy() final override;
    void AcquireImage() final override;
    void PresentImage() final override;

    void CopyToSwapchain(const Shared<Framebuffer>& framebuffer) final override;
};

}  // namespace Pathfinder

#endif  // VULKANSWAPCHAIN_H
