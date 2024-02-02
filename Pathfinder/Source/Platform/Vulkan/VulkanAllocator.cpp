#include "PathfinderPCH.h"
#include "VulkanAllocator.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "Renderer/Renderer.h"

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

    if (Renderer::GetRendererSettings().bBDASupport) allocatorCI.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_Handle), "Failed to create VMA!");
}

void VulkanAllocator::CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation, VmaMemoryUsage memoryUsage)
{
    VmaAllocationInfo allocationInfo = {};

    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.usage                   = memoryUsage;
    allocationCI.flags                   = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocationCI.requiredFlags           = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocationCI.preferredFlags          = allocationCI.requiredFlags;

    auto allocationResult = vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, &allocationInfo);
    if (allocationResult == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
        PFR_ASSERT(false, "RAM limit reached!");
    }
    else if (allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        // Try to allocate on host.
        allocationCI       = {};
        allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocationResult   = vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, &allocationInfo);
        VK_CHECK(allocationResult, "Overall memory limit reached!");
    }
    else
        VK_CHECK(allocationResult, "Failed to create image!");

    LOG_DEBUG("[VMA]: Created image with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
}

void VulkanAllocator::CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation,
                                   VmaMemoryUsage memoryUsage)
{
    VmaAllocationInfo allocationInfo = {};

    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.usage                   = memoryUsage;
    if (bufferCI.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    auto allocationResult = vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, &allocationInfo);
    if (allocationResult == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
        PFR_ASSERT(false, "RAM limit reached!");
    }
    else if (allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        // Try to allocate on host.
        allocationCI       = {};
        allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocationResult   = vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, &allocationInfo);
        VK_CHECK(allocationResult, "Overall memory limit reached!");
    }
    else
        VK_CHECK(allocationResult, "Failed to create buffer!");

    LOG_DEBUG("[VMA]: Created buffer with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
}

void VulkanAllocator::DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    VmaAllocationInfo allocationInfo = {};
    vmaGetAllocationInfo(m_Handle, allocation, &allocationInfo);

    vmaDestroyBuffer(m_Handle, buffer, allocation);

    LOG_DEBUG("[VMA]: Destroyed buffer with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
}

void VulkanAllocator::DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    VmaAllocationInfo allocationInfo = {};
    vmaGetAllocationInfo(m_Handle, allocation, &allocationInfo);

    vmaDestroyImage(m_Handle, image, allocation);

    LOG_DEBUG("[VMA]: Destroyed image with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
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