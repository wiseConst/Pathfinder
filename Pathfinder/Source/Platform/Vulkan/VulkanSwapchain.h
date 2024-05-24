#pragma once

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

    void SetClearColor(const glm::vec3& clearColor) final override;
    void SetVSync(bool bVSync) final override
    {
        if (bVSync && m_PresentMode == EPresentMode::PRESENT_MODE_FIFO || !bVSync && m_PresentMode == EPresentMode::PRESENT_MODE_MAILBOX)
            return;

        m_PresentMode    = bVSync ? EPresentMode::PRESENT_MODE_FIFO : EPresentMode::PRESENT_MODE_MAILBOX;
        m_bNeedsRecreate = true;
    }
    void SetWindowMode(const EWindowMode windowMode) final override;

    void SetPresentMode(const EPresentMode presentMode) final override
    {
        if (m_PresentMode == presentMode) return;

        m_PresentMode    = presentMode;
        m_bNeedsRecreate = true;
    }

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

#if PFR_WINDOWS
    VkSurfaceFullScreenExclusiveWin32InfoEXT m_SurfaceFullScreenExclusiveWin32Info = {
        VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT};
#endif
    EWindowMode m_LastWindowMode                                         = EWindowMode::WINDOW_MODE_WINDOWED;
    int32_t m_LastPosX                                                   = 0;
    int32_t m_LastPosY                                                   = 0;
    int32_t m_LastWidth                                                  = 1280;
    int32_t m_LastHeight                                                 = 720;
    VkSurfaceFullScreenExclusiveInfoEXT m_SurfaceFullScreenExclusiveInfo = {VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT};

    VkSurfaceKHR m_Surface                = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_ImageFormat      = {VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR m_CurrentPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkExtent2D m_ImageExtent = {1280, 720};

    uint32_t m_ImageIndex{0};
    uint32_t m_FrameIndex{0};

    EWindowMode m_WindowMode = EWindowMode::WINDOW_MODE_WINDOWED;
    bool m_bNeedsRecreate    = false;

    void Recreate();
    void Destroy() final override;
    bool AcquireImage() final override;
    void PresentImage() final override;

    void BeginPass(const Shared<CommandBuffer>& commandBuffer, const bool bPreserveContents) final override;
    void EndPass(const Shared<CommandBuffer>& commandBuffer) final override;

    void CopyToSwapchain(const Shared<Image>& image) final override;
};

}  // namespace Pathfinder
