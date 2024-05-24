#pragma once

#include "VulkanCore.h"
#include <vector>

namespace Pathfinder
{

using DescriptorSet         = std::pair<uint32_t, VkDescriptorSet>;
using DescriptorSetPerFrame = std::array<DescriptorSet, s_FRAMES_IN_FLIGHT>;

class VulkanDescriptorAllocator final : private Unmovable, private Uncopyable
{
  public:
    explicit VulkanDescriptorAllocator(VkDevice& device);
    ~VulkanDescriptorAllocator() { Destroy(); }

    NODISCARD bool Allocate(DescriptorSet& outDescriptorSet, const VkDescriptorSetLayout& descriptorSetLayout);
    void ReleaseDescriptorSets(DescriptorSet* descriptorSets, const uint32_t descriptorSetCount);

    FORCEINLINE const auto GetAllocatedDescriptorSetsCount() const { return m_AllocatedDescriptorSets; }

  private:
    std::mutex m_Mutex;
#define VDA_LOCK_GUARD_MUTEX std::lock_guard lock(m_Mutex)

    static inline std::vector<std::pair<VkDescriptorType, float>> s_DefaultPoolSizes = {
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
    };

    std::vector<VkDescriptorPool> m_Pools;

    VkDevice& m_LogicalDevice;
    VkDescriptorPool m_CurrentPool          = VK_NULL_HANDLE;
    const uint32_t m_BasePoolSizeMultiplier = 250;
    uint32_t m_CurrentPoolSizeMultiplier    = m_BasePoolSizeMultiplier;
    uint32_t m_AllocatedDescriptorSets      = 0;

    NODISCARD VkDescriptorPool CreatePool(const uint32_t count, VkDescriptorPoolCreateFlags descriptorPoolCreateFlags = 0);
    void Destroy();
    VulkanDescriptorAllocator() = delete;
};

}  // namespace Pathfinder
