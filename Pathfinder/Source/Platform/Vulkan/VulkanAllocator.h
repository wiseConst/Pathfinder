#pragma once

#include "VulkanCore.h"
#define VK_NO_PROTOTYPES
#include <vma/vk_mem_alloc.h>

namespace Pathfinder
{

class VulkanAllocator final : private Uncopyable, private Unmovable
{
  public:
    VulkanAllocator(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice);
    ~VulkanAllocator();

    void CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation, const BufferFlags extraFlags);
    void CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation,
                     VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY);

    void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation);
    void DestroyImage(VkImage& image, VmaAllocation& allocation);

    bool IsAllocationMappable(const VmaAllocation& allocation) const;
    NODISCARD void* Map(VmaAllocation& allocation);
    void Unmap(VmaAllocation& allocation);

    void SetCurrentFrameIndex(const uint32_t frameIndex);
    void FillMemoryBudgetStats(std::vector<MemoryBudget>& memoryBudgets) const;

  private:
    VmaAllocator m_Handle = VK_NULL_HANDLE;
    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> m_MemoryBudgets;
};

}  // namespace Pathfinder
