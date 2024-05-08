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

    VmaAllocatorCreateInfo allocatorCI = {VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT |
                                              VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT,
                                          physicalDevice, device};
    allocatorCI.instance               = VulkanContext::Get().GetInstance();
    allocatorCI.vulkanApiVersion       = PFR_VK_API_VERSION;
    allocatorCI.pVulkanFunctions       = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_Handle), "Failed to create VMA!");
    m_MemoryBudgets.fill({{0, 0, 0, 0}, 0, 0});
}

void VulkanAllocator::CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation, VmaMemoryUsage memoryUsage)
{
    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.usage                   = memoryUsage;
    allocationCI.flags                   = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocationCI.requiredFlags           = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    allocationCI.preferredFlags          = allocationCI.requiredFlags;
    allocationCI.priority                = 1.0f;

    VmaAllocationInfo allocationInfo = {};

    auto allocationResult = vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, &allocationInfo);
    if (allocationResult == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
        PFR_ASSERT(false, "RAM limit reached!");
    }
    else if (allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        // Try to allocate on host.
        allocationCI          = {};
        allocationCI.priority = 1.0f;
        allocationCI.usage    = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocationResult      = vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, &allocationInfo);
        VK_CHECK(allocationResult, "Overall memory limit reached!");
    }
    else
        VK_CHECK(allocationResult, "Failed to create image!");

    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Created image with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

void VulkanAllocator::CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation,
                                   VmaMemoryUsage memoryUsage)
{
    VmaAllocationInfo allocationInfo = {};

    VmaAllocationCreateInfo allocationCI = {};
    allocationCI.priority                = 1.0f;
    allocationCI.usage                   = memoryUsage;
    if (bufferCI.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) allocationCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (bufferCI.usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)  // Try allocate on host & device memory(if fails it prefers device)
    {
        allocationCI.flags |=
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
    }

    auto allocationResult = vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, &allocationInfo);
    if (allocationResult == VK_ERROR_OUT_OF_HOST_MEMORY)
    {
        PFR_ASSERT(false, "RAM limit reached!");
    }
    else if (allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY)
    {
        // Try to allocate on host.
        allocationCI          = {};
        allocationCI.priority = 1.0f;
        allocationCI.usage    = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocationResult      = vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, &allocationInfo);
        VK_CHECK(allocationResult, "Overall memory limit reached!");
    }
    else
        VK_CHECK(allocationResult, "Failed to create buffer!");

    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Created buffer with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

void VulkanAllocator::DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    VmaAllocationInfo allocationInfo = {};
    vmaGetAllocationInfo(m_Handle, allocation, &allocationInfo);

    vmaDestroyBuffer(m_Handle, buffer, allocation);
    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Destroyed buffer with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

void VulkanAllocator::DestroyImage(VkImage& image, VmaAllocation& allocation)
{
    VmaAllocationInfo allocationInfo = {};
    vmaGetAllocationInfo(m_Handle, allocation, &allocationInfo);

    vmaDestroyImage(m_Handle, image, allocation);
    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Destroyed image with offset: %zu (bytes), size: %0.6f (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

void* VulkanAllocator::GetMapped(const VmaAllocation& allocation) const
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

void VulkanAllocator::FillMemoryBudgetStats(std::vector<MemoryBudget>& memoryBudgets) const
{
    memoryBudgets.clear();

    for (const auto& vmaMemoryBudget : m_MemoryBudgets)
    {
        if (vmaMemoryBudget.budget == 0) continue;

        memoryBudgets.emplace_back(vmaMemoryBudget.statistics.blockCount, vmaMemoryBudget.statistics.allocationCount,
                                   vmaMemoryBudget.statistics.blockBytes, vmaMemoryBudget.statistics.allocationBytes, vmaMemoryBudget.usage,
                                   vmaMemoryBudget.budget);
    }

    memoryBudgets.shrink_to_fit();
}

bool VulkanAllocator::IsMappable(const VmaAllocation& allocation)
{
    VkMemoryPropertyFlags memPropFlags = {};
    vmaGetAllocationMemoryProperties(m_Handle, allocation, &memPropFlags);

    return memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

void VulkanAllocator::SetCurrentFrameIndex(const uint32_t frameIndex)
{
    vmaSetCurrentFrameIndex(m_Handle, frameIndex);
}

VulkanAllocator::~VulkanAllocator()
{
    vmaDestroyAllocator(m_Handle);
}

}  // namespace Pathfinder