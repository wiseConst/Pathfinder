#include "PathfinderPCH.h"
#include "VulkanSwapchain.h"
#include "VulkanContext.h"

namespace Pathfinder
{
void VulkanSwapchain::Invalidate()
{
    auto& context = VulkanContext::Get();

    auto oldSwapchain = m_Handle;
    // VkSwapchainCreateInfoKHR swapchainCI={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};

    // vkCreateSwapchainKHR(,&swapchainCI,nullptr,&m_Handle);

    // if(oldSwapchain) vkDestroySwapchainKHR()
}

void VulkanSwapchain::AcquireImage()
{
    // const auto result =
    //     vkAcquireNextImageKHR(m_Device->GetLogicalDevice(), m_Swapchain, UINT64_MAX, imageAcquiredSemaphore, fence, &m_ImageIndex);
    // if (result == VK_SUCCESS) return true;
    //
    // if (result != VK_SUBOPTIMAL_KHR || result != VK_ERROR_OUT_OF_DATE_KHR)
    // {
    //     const auto ResultMessage =
    //         std::string("Failed to acquire image from the swapchain! Result is: ") + std::string(GetStringVulkanResult(result));
    //     GNT_ASSERT(false, ResultMessage.data());
    // }
    //
    // Recreate();
}

void VulkanSwapchain::PresentImage()
{
    // VkPresentInfoKHR presentInfo   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    // presentInfo.waitSemaphoreCount = 1;
    // presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;
    // presentInfo.pImageIndices      = &m_ImageIndex;
    // presentInfo.swapchainCount     = 1;
    // presentInfo.pSwapchains        = &m_Swapchain;
    //
    // const float imagePresentBegin = static_cast<float>(Timer::Now());
    //
    // const auto result = vkQueuePresentKHR(m_Device->GetPresentQueue(), &presentInfo);
    //
    // const float imagePresentEnd      = static_cast<float>(Timer::Now());
    // Renderer::GetStats().PresentTime = imagePresentEnd - imagePresentBegin;
    //
    // if (result == VK_SUCCESS)
    // {
    //     m_FrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;
    //     return;
    // }
    //
    // Recreate();
}

void VulkanSwapchain::Destroy()
{
    // if(m_Handle) vkDestroySwapchainKHR()
}

}  // namespace Pathfinder