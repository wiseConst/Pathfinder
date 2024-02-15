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

    NODISCARD void* GetMapped(VmaAllocation& allocation);
    NODISCARD void* Map(VmaAllocation& allocation);
    void Unmap(VmaAllocation& allocation);

  private:
    VmaAllocator m_Handle = VK_NULL_HANDLE;
};

}  // namespace Pathfinder

#endif  // VULKANALLOCATOR_H
