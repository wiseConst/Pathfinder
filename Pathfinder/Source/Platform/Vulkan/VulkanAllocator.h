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

  private:
    VmaAllocator m_Handle = VK_NULL_HANDLE;

    struct VmaAllocations
    {

    };
};

}  // namespace Pathfinder

#endif  // VULKANALLOCATOR_H
