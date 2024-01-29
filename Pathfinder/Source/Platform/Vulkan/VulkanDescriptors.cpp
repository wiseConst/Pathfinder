#include "PathfinderPCH.h"
#include "VulkanDescriptors.h"

#include "VulkanDevice.h"

#include "Renderer/Renderer.h"

namespace Pathfinder
{

VulkanDescriptorAllocator::VulkanDescriptorAllocator(VkDevice& device) : m_LogicalDevice(device) {}

bool VulkanDescriptorAllocator::Allocate(DescriptorSet& outDescriptorSet, const VkDescriptorSetLayout& descriptorSetLayout)
{
    VDA_LOCK_GUARD_MUTEX;

    // Initialize the currentPool handle if it's null.
    if (m_CurrentPool == VK_NULL_HANDLE)
    {
        // clang-format off
        if (m_Pools.size() > 0) m_CurrentPool = m_Pools.back();
        else
        {
            m_CurrentPool = CreatePool(m_CurrentPoolSizeMultiplier);
            m_Pools.push_back(m_CurrentPool);
            m_CurrentPoolSizeMultiplier = static_cast<uint32_t>(m_CurrentPoolSizeMultiplier * 1.3f);
        }
        // clang-format on
    }

    // Try to allocate the descriptor set with current.
    VkResult allocResult = VK_RESULT_MAX_ENUM;
    {
        const auto currentDescriptorSetAllocateInfo = VulkanUtility::GetDescriptorSetAllocateInfo(m_CurrentPool, 1, &descriptorSetLayout);
        allocResult = vkAllocateDescriptorSets(m_LogicalDevice, &currentDescriptorSetAllocateInfo, &outDescriptorSet.second);
        if (allocResult == VK_SUCCESS)
        {
            ++m_AllocatedDescriptorSets;
            outDescriptorSet.first = static_cast<uint32_t>(m_Pools.size()) - 1;
            ++Renderer::GetStats().DescriptorSetCount;
            return true;
        }
    }

    // Try different pools.
    for (size_t i = 0; i < m_Pools.size(); ++i)
    {
        if (m_Pools[i] == m_CurrentPool) continue;  // Skip current cuz it's checked already.

        // Try to allocate the descriptor set with new pool.
        const auto newDescriptorSetAllocateInfo = VulkanUtility::GetDescriptorSetAllocateInfo(m_Pools[i], 1, &descriptorSetLayout);
        allocResult = vkAllocateDescriptorSets(m_LogicalDevice, &newDescriptorSetAllocateInfo, &outDescriptorSet.second);
        if (allocResult == VK_SUCCESS)
        {
            ++m_AllocatedDescriptorSets;
            outDescriptorSet.first = i;
            ++Renderer::GetStats().DescriptorSetCount;
            return true;
        }
    }

    // Allocation through all pools failed, try to get new pool and allocate using this new pool.
    if (allocResult == VK_ERROR_FRAGMENTED_POOL || allocResult == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        m_CurrentPool = CreatePool(m_CurrentPoolSizeMultiplier);
        m_Pools.push_back(m_CurrentPool);
        m_CurrentPoolSizeMultiplier = static_cast<uint32_t>(m_CurrentPoolSizeMultiplier * 1.3f);

        // If it still fails then we have big issues.
        const auto newDescriptorSetAllocateInfo = VulkanUtility::GetDescriptorSetAllocateInfo(m_CurrentPool, 1, &descriptorSetLayout);
        allocResult = vkAllocateDescriptorSets(m_LogicalDevice, &newDescriptorSetAllocateInfo, &outDescriptorSet.second);
        if (allocResult == VK_SUCCESS)
        {
            ++m_AllocatedDescriptorSets;
            outDescriptorSet.first = static_cast<uint32_t>(m_Pools.size()) - 1;
            ++Renderer::GetStats().DescriptorSetCount;
            return true;
        }
    }

    PFR_ASSERT(false, "Failed to allocate descriptor pool!");
    return false;
}

void VulkanDescriptorAllocator::ReleaseDescriptorSets(DescriptorSet* descriptorSets, const uint32_t descriptorSetCount)
{
    VDA_LOCK_GUARD_MUTEX;

    // Since descriptor sets can be allocated through different pool I have to iterate them like dumb.
    Renderer::GetStats().DescriptorSetCount -= descriptorSetCount;
    for (uint32_t i = 0; i < descriptorSetCount; ++i)
    {
        PFR_ASSERT(descriptorSets[i].first < UINT32_MAX, "Invalid Pool ID!");
        if (!descriptorSets[i].second)
        {
            LOG_TAG_WARN(VULKAN, "Failed to release descriptor set!");
            continue;
        }

        VK_CHECK(vkFreeDescriptorSets(m_LogicalDevice, m_Pools[descriptorSets[i].first], 1, &descriptorSets[i].second),
                 "Failed to free descriptor set!");
        descriptorSets[i].second = VK_NULL_HANDLE;
        --m_AllocatedDescriptorSets;
    }
}

VkDescriptorPool VulkanDescriptorAllocator::CreatePool(const uint32_t count, VkDescriptorPoolCreateFlags descriptorPoolCreateFlags)
{
    std::vector<VkDescriptorPoolSize> poolSizes(s_DefaultPoolSizes.size());

    for (size_t i = 0; i < poolSizes.size(); ++i)
    {
        poolSizes[i].type            = s_DefaultPoolSizes[i].first;
        poolSizes[i].descriptorCount = static_cast<uint32_t>(s_DefaultPoolSizes[i].second * count);
        if (poolSizes[i].descriptorCount > 4096)
        {
            LOG_TAG_WARN(VULKAN, "Descriptor count exceeded limit > 4096!");
            poolSizes[i].descriptorCount = 4096;
        }
    }

    const auto descriptorPoolCreateInfo = VulkanUtility::GetDescriptorPoolCreateInfo(
        static_cast<uint32_t>(poolSizes.size()), count, poolSizes.data(),
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | descriptorPoolCreateFlags);

    VkDescriptorPool newDescriptorPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(m_LogicalDevice, &descriptorPoolCreateInfo, nullptr, &newDescriptorPool),
             "Failed to create descriptor pool!");

    Renderer::GetStats().DescriptorPoolCount += 1;
    return newDescriptorPool;
}

void VulkanDescriptorAllocator::Destroy()
{
    VDA_LOCK_GUARD_MUTEX;

    // No need to reset pools since it's done by impl
    Renderer::GetStats().DescriptorPoolCount -= m_Pools.size();
    std::ranges::for_each(m_Pools, [&](auto& pool) { vkDestroyDescriptorPool(m_LogicalDevice, pool, nullptr); });
    m_Pools.clear();

    m_CurrentPool               = VK_NULL_HANDLE;
    m_AllocatedDescriptorSets   = 0;
    m_CurrentPoolSizeMultiplier = m_BasePoolSizeMultiplier;
}

}  // namespace Pathfinder