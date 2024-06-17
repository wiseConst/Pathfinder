#include <PathfinderPCH.h>
#include "VulkanAllocator.h"

#include <Renderer/Renderer.h>

#define VK_NO_PROTOTYPES
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

namespace Pathfinder
{

VulkanAllocator::VulkanAllocator(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
    const VmaVulkanFunctions vulkanFunctions = {.vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

    const VmaAllocatorCreateInfo allocatorCI = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT |
                 VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT,
        .physicalDevice   = physicalDevice,
        .device           = device,
        .pVulkanFunctions = &vulkanFunctions,
        .instance         = instance,
        .vulkanApiVersion = PFR_VK_API_VERSION};

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_Handle), "Failed to create VMA!");
    m_MemoryBudgets.fill({{0, 0, 0, 0}, 0, 0});
}

void VulkanAllocator::CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation, VmaMemoryUsage memoryUsage)
{
    const VmaAllocationCreateInfo allocationCI = {.flags         = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                                  .usage         = memoryUsage,
                                                  .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                  .priority      = 1.f};

    VmaAllocationInfo allocationInfo = {};
    VK_CHECK(vmaCreateImage(m_Handle, &imageCI, &allocationCI, &image, &allocation, &allocationInfo), "Failed to create image!");
    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Created image with offset: {} (bytes), size: {:.6f} (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

void VulkanAllocator::CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation,
                                   const BufferFlags extraFlags)
{
    PFR_ASSERT(extraFlags != 0, "Can't create buffer with extraFlags == 0!");
    const bool bIsDeviceLocal = (extraFlags & EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL) == EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL;
    constexpr VmaAllocationCreateFlags vmaResizableBARFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;  // ReBAR flags as stated in VMA Advanced Tips.
    const VmaAllocationCreateInfo allocationCI = {
        .flags         = bIsDeviceLocal ? vmaResizableBARFlags : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage         = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = bIsDeviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VkMemoryPropertyFlags{0},
        .priority      = 1.f};

    VmaAllocationInfo allocationInfo = {};
    VK_CHECK(vmaCreateBuffer(m_Handle, &bufferCI, &allocationCI, &buffer, &allocation, &allocationInfo), "Failed to create buffer!");
    vmaGetHeapBudgets(m_Handle, m_MemoryBudgets.data());

#if VK_LOG_VMA_ALLOCATIONS
    LOG_DEBUG("[VMA]: Created buffer with offset: {} (bytes), size: {:.6f} (MB).", allocationInfo.offset,
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
    LOG_DEBUG("[VMA]: Destroyed buffer with offset: {} (bytes), size: {:.6f} (MB).", allocationInfo.offset,
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
    LOG_DEBUG("[VMA]: Destroyed image with offset: {} (bytes), size: {:.6f} (MB).", allocationInfo.offset,
              static_cast<float>(allocationInfo.size) / 1024.0f / 1024.0f);
#endif
}

bool VulkanAllocator::IsAllocationMappable(const VmaAllocation& allocation) const
{
    VkMemoryPropertyFlags memPropFlags = {};
    vmaGetAllocationMemoryProperties(m_Handle, allocation, &memPropFlags);

    return (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
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

void VulkanAllocator::SetCurrentFrameIndex(const uint32_t frameIndex)
{
    vmaSetCurrentFrameIndex(m_Handle, frameIndex);
}

VulkanAllocator::~VulkanAllocator()
{
    vmaDestroyAllocator(m_Handle);
}

}  // namespace Pathfinder