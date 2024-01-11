#include "PathfinderPCH.h"
#include "VulkanFramebuffer.h"

#include "VulkanCommandBuffer.h"

#include "Core/Application.h"
#include "Core/Window.h"

namespace Pathfinder
{

VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& framebufferSpec) : m_Specification(framebufferSpec)
{
    if (m_Specification.Width == 0 || m_Specification.Height == 0)
    {
        m_Specification.Width  = Application::Get().GetWindow()->GetSpecification().Width;
        m_Specification.Height = Application::Get().GetWindow()->GetSpecification().Height;
    }

    Invalidate();
}

void VulkanFramebuffer::BeginPass(const Shared<CommandBuffer>& commandBuffer)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea={{0,0},{m_Specification.Width,m_Specification.Height}};

    vulkanCommandBuffer->BeginRendering(&renderingInfo);
}

void VulkanFramebuffer::EndPass(const Shared<CommandBuffer>& commandBuffer)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    vulkanCommandBuffer->EndRendering();
}

void VulkanFramebuffer::Invalidate() {}

void VulkanFramebuffer::Destroy() {}

}  // namespace Pathfinder