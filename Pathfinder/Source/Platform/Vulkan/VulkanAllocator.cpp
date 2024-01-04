#include "PathfinderPCH.h"
#include "VulkanAllocator.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

namespace Pathfinder
{

VulkanAllocator::VulkanAllocator(const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
    VmaVulkanFunctions vulkanFunctions    = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCI = {};
    allocatorCI.device                 = device;
    allocatorCI.physicalDevice         = physicalDevice;
    allocatorCI.instance               = VulkanContext::Get().GetInstance();
    allocatorCI.pVulkanFunctions       = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_Handle), "Failed to create VMA!");
}

VulkanAllocator::~VulkanAllocator()
{
    vmaDestroyAllocator(m_Handle);
}

}  // namespace Pathfinder