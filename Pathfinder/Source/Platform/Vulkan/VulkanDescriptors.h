#ifndef VULKANDESCRIPTORS_H
#define VULKANDESCRIPTORS_H

#include "VulkanCore.h"
#include <vector>

namespace Pathfinder
{

using DescriptorSet         = std::pair<uint32_t, VkDescriptorSet>;
using DescriptorSetPerFrame = std::array<DescriptorSet, s_FRAMES_IN_FLIGHT>;

class VulkanDescriptorAllocator final : private Unmovable, private Uncopyable
{
  public:
    VulkanDescriptorAllocator(VkDevice& device);
    VulkanDescriptorAllocator()           = delete;
    ~VulkanDescriptorAllocator() override = default;

    NODISCARD bool Allocate(DescriptorSet& outDescriptorSet, const VkDescriptorSetLayout& descriptorSetLayout);
    void ReleaseDescriptorSets(DescriptorSet* descriptorSets, const uint32_t descriptorSetCount);

    void Destroy();

    FORCEINLINE const uint32_t GetAllocatedDescriptorSetsCount() { return m_AllocatedDescriptorSets; }

  private:
    VkDevice& m_LogicalDevice;

    std::vector<VkDescriptorPool> m_Pools;
    VkDescriptorPool m_CurrentPool          = VK_NULL_HANDLE;
    const uint32_t m_BasePoolSizeMultiplier = 250;
    uint32_t m_CurrentPoolSizeMultiplier    = m_BasePoolSizeMultiplier;
    uint32_t m_AllocatedDescriptorSets      = 0;

    std::mutex m_Mutex;
#define VDA_LOCK_GUARD_MUTEX std::lock_guard lock(m_Mutex)

    static inline constexpr std::vector<std::pair<VkDescriptorType, float>> s_DefaultPoolSizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},                 //
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},   //
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},            //
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},            //
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0.5f},    //
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0.5f},    //
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.5f},          //
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0.5f},  //
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},           //
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0.5f},  //
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}         //
    };                                                      //

    NODISCARD VkDescriptorPool CreatePool(const uint32_t count, VkDescriptorPoolCreateFlags descriptorPoolCreateFlags = 0);
};

}  // namespace Pathfinder

#endif  // VULKANDESCRIPTORS_H
