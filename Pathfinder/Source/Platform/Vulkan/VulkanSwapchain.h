#ifndef VULKANSWAPCHAIN_H
#define VULKANSWAPCHAIN_H

#include "Renderer/Swapchain.h"
#include <volk.h>

namespace Pathfinder
{

class VulkanSwapchain final : public Swapchain
{
  public:
    VulkanSwapchain() noexcept { Invalidate(); }
    virtual ~VulkanSwapchain() override = default;

    void AcquireImage() final override;
    void PresentImage() final override;
    void Invalidate() final override;
    void Destroy() final override;

  private:
    VkSwapchainKHR m_Handle = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface  = VK_NULL_HANDLE;
};

}  // namespace Pathfinder

#endif  // VULKANSWAPCHAIN_H
