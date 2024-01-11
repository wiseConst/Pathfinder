#ifndef VULKANDESCRIPTORS_H
#define VULKANDESCRIPTORS_H

#include "VulkanCore.h"

namespace Pathfinder
{

using DescriptorSet = std::pair<uint32_t, VkDescriptorSet>;

class VulkanDescriptorAllocator final : private Uncopyable, private Unmovable
{
  public:
    VulkanDescriptorAllocator()           = default;
    ~VulkanDescriptorAllocator() override = default;

  private:
};

}  // namespace Pathfinder

#endif  // VULKANDESCRIPTORS_H
