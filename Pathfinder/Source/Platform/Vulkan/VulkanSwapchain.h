#ifndef VULKANSWAPCHAIN_H
#define VULKANSWAPCHAIN_H

#include "Renderer/Swapchain.h"
#include "VulkanCore.h"
#include <vector>

namespace Pathfinder
{
class VulkanCommandBuffer;

class VulkanSwapchain final : public Swapchain
{
  public:
    VulkanSwapchain(void* windowHandle) noexcept;
    virtual ~VulkanSwapchain() override { Destroy(); }

    NODISCARD const EImageFormat GetImageFormat() const final override;
    NODISCARD FORCEINLINE const uint32_t GetImageCount() const final override { return m_Images.size(); }
    NODISCARD FORCEINLINE const uint32_t GetCurrentFrameIndex() const final override { return m_FrameIndex; }
    NODISCARD FORCEINLINE void* GetImageAvailableSemaphore() const final override { return m_ImageAcquiredSemaphore[m_FrameIndex]; }
    NODISCARD FORCEINLINE void* GetRenderFence() const final override { return m_RenderFence[m_FrameIndex]; }
    NODISCARD FORCEINLINE void* GetRenderSemaphore() const final override { return m_RenderSemaphore[m_FrameIndex]; }
    NODISCARD FORCEINLINE bool IsVSync() const final override { return m_bVSync; }

    void SetClearColor(const glm::vec3& clearColor) final override;
    void SetVSync(bool bVSync) final override
    {
        if (m_bVSync == bVSync) return;

        m_bVSync = bVSync;
        Recreate();
        LOG_DEBUG("VSync: %s.", m_bVSync ? "ON" : "OFF");
    }
    void SetWindowMode(const EWindowMode windowMode) final override;

    void Invalidate() final override;

    FORCEINLINE void AddResizeCallback(ResizeCallback&& resizeCallback) final override
    {
        m_ResizeCallbacks.emplace_back(std::forward<ResizeCallback>(resizeCallback));
    }

  private:
    VkSwapchainKHR m_Handle = VK_NULL_HANDLE;
    void* m_WindowHandle    = nullptr;

    VulkanSemaphorePerFrame m_RenderSemaphore;
    VulkanFencePerFrame m_RenderFence;
    VulkanSemaphorePerFrame m_ImageAcquiredSemaphore;

    std::vector<VkImageLayout> m_ImageLayouts;
    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;

#if PFR_WINDOWS && VK_EXCLUSIVE_FULL_SCREEN_TEST
    VkSurfaceFullScreenExclusiveWin32InfoEXT m_SurfaceFullScreenExclusiveWin32Info = {
        VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT};
    VkSurfaceFullScreenExclusiveInfoEXT m_SurfaceFullScreenExclusiveInfo = {VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT};
#endif

    VkSurfaceKHR m_Surface           = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_ImageFormat = {VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR};

    VkPresentModeKHR m_CurrentPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D m_ImageExtent              = {1280, 720};

    uint32_t m_ImageIndex{0};
    uint32_t m_FrameIndex{0};

    EWindowMode m_WindowMode = EWindowMode::WINDOW_MODE_WINDOWED;
    bool m_bWasInvalidated   = false;

    NODISCARD FORCEINLINE bool WasInvalidatedDuringCurrentFrame() const final override { return m_bWasInvalidated; }

    void Recreate();
    void Destroy() final override;
    bool AcquireImage() final override;
    void PresentImage() final override;

    void BeginPass(const Shared<CommandBuffer>& commandBuffer, const bool bPreserveContents) final override;
    void EndPass(const Shared<CommandBuffer>& commandBuffer) final override;

    void CopyToSwapchain(const Shared<Image>& image) final override;
};

}  // namespace Pathfinder

#endif  // VULKANSWAPCHAIN_H
