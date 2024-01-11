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

    VmaAllocatorCreateInfo allocatorCI = {0, physicalDevice, device};
    allocatorCI.instance               = VulkanContext::Get().GetInstance();
    allocatorCI.vulkanApiVersion       = VK_API_VERSION_1_3;
    allocatorCI.pVulkanFunctions       = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_Handle), "Failed to create VMA!");
}

void VulkanAllocator::CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation, VmaMemoryUsage memoryUsage)
{
    // VmaAllocationInfo allocationInfo;

    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.usage = memoryUsage;
    //allocationCI.flags         = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocationCI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, VK_NULL_HANDLE /* &allocationInfo*/),
             "Failed to create image!");
}

void VulkanAllocator::CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation,
                                   VmaMemoryUsage memoryUsage)
{
    // VmaAllocationInfo allocationInfo;

    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.usage = memoryUsage;

    VK_CHECK(vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, VK_NULL_HANDLE /* &allocationInfo */),
             "Failed to create buffer!");
}

void VulkanAllocator::DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    vmaDestroyBuffer(m_Handle, buffer, allocation);
}

void VulkanAllocator::DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    vmaDestroyImage(m_Handle, image, allocation);
}

void* VulkanAllocator::GetMapped(VmaAllocation& allocation)
{
    return allocation->GetMappedData();
}

void* VulkanAllocator::Map(VmaAllocation& allocation)
{
    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(m_Handle, allocation, &mapped), "Failed to map allocation!");
    return mapped;
}

void VulkanAllocator::Unmap(VmaAllocation& allocation)
{
    vmaUnmapMemory(m_Handle, allocation);
}

VulkanAllocator::~VulkanAllocator()
{
    vmaDestroyAllocator(m_Handle);
}

}  // namespace Pathfinder