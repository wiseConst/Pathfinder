#ifndef VULKANALLOCATOR_H
#define VULKANALLOCATOR_H

#include "VulkanCore.h"
#define VK_NO_PROTOTYPES
#include <vma/vk_mem_alloc.h>

namespace Pathfinder
{

class VulkanDevice;

class VulkanAllocator final : private Uncopyable, private Unmovable
{
  public:
    VulkanAllocator(const VkDevice& device, const VkPhysicalDevice& physicalDevice);
    ~VulkanAllocator() override;

    void CreateImage(const VkImageCreateInfo& imageCI, VkImage& image, VmaAllocation& allocation,
                     VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY);

    void CreateBuffer(const VkBufferCreateInfo& bufferCI, VkBuffer& buffer, VmaAllocation& allocation,
                      VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY);

    void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation);
    void DestroyImage(VkImage& image, VmaAllocation& allocation);

    bool IsMappable(const VmaAllocation& allocation);

    void SetCurrentFrameIndex(const uint32_t frameIndex);

    NODISCARD void* GetMapped(const VmaAllocation& allocation) const;
    NODISCARD void* Map(VmaAllocation& allocation);
    void Unmap(VmaAllocation& allocation);

    void FillMemoryBudgetStats(std::vector<MemoryBudget>& memoryBudgets) const;

  private:
    VmaAllocator m_Handle = VK_NULL_HANDLE;
    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> m_MemoryBudgets;
};

}  // namespace Pathfinder

#endif  // VULKANALLOCATOR_H
