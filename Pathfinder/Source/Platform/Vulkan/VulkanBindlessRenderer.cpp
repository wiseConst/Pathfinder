#include "PathfinderPCH.h"
#include "VulkanBindlessRenderer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

namespace Pathfinder
{

VulkanBindlessRenderer::VulkanBindlessRenderer()
{
    CreateDescriptorPools();
    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer created! (NOT IMPLEMENTED CURRENTLY)");
}

void VulkanBindlessRenderer::CreateDescriptorPools() {}

void VulkanBindlessRenderer::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    vkDestroyDescriptorPool(logicalDevice, m_GlobalTexturesPool, nullptr);

    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer destroyed! (NOT IMPLEMENTED CURRENTLY)");
}

}  // namespace Pathfinder