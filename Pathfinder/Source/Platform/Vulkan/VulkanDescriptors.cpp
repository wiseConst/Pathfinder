#include <PathfinderPCH.h>
#include "VulkanDescriptors.h"

#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanTexture.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Renderer.h>

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
        const auto currentDescriptorSetAllocateInfo = VulkanUtils::GetDescriptorSetAllocateInfo(m_CurrentPool, 1, &descriptorSetLayout);
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
    for (size_t i{}; i < m_Pools.size(); ++i)
    {
        if (m_Pools[i] == m_CurrentPool) continue;  // Skip current cuz it's checked already.

        // Try to allocate the descriptor set with new pool.
        const auto newDescriptorSetAllocateInfo = VulkanUtils::GetDescriptorSetAllocateInfo(m_Pools[i], 1, &descriptorSetLayout);
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
        const auto newDescriptorSetAllocateInfo = VulkanUtils::GetDescriptorSetAllocateInfo(m_CurrentPool, 1, &descriptorSetLayout);
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
            LOG_WARN("Failed to release descriptor set!");
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
            LOG_WARN("Descriptor count exceeded limit > 4096!");
            poolSizes[i].descriptorCount = 4096;
        }
    }

    const auto descriptorPoolCreateInfo = VulkanUtils::GetDescriptorPoolCreateInfo(
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

VulkanDescriptorManager::VulkanDescriptorManager()
{
    CreateDescriptorPools();
    LOG_INFO("Vulkan Descriptor Manager created!");
}

void VulkanDescriptorManager::Bind(const Shared<CommandBuffer>& commandBuffer, const EPipelineStage overrideBindPoint)
{
    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();

    VkPipelineBindPoint pipelineBindPoint = commandBuffer->GetSpecification().Type == ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL
                                                ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                                : VK_PIPELINE_BIND_POINT_COMPUTE;
    if (overrideBindPoint != EPipelineStage::PIPELINE_STAGE_NONE)
    {
        if (overrideBindPoint == EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT)
            pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        else if (overrideBindPoint == EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT)
            pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        else if (overrideBindPoint == EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT)
            pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        else
            PFR_ASSERT(false, "Unknown pipeline stages to override pipeline bind point for descriptor sets!");
    }

    vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), pipelineBindPoint, m_MegaPipelineLayout, 0, 1, &m_MegaSet[currentFrame],
                            0, nullptr);
}

void VulkanDescriptorManager::LoadImage(const void* pImageInfo, Optional<uint32_t>& outIndex)
{
    const VkDescriptorImageInfo* vkImageInfo = (const VkDescriptorImageInfo*)pImageInfo;
    PFR_ASSERT(pImageInfo && vkImageInfo->imageView, "VulkanDescriptorManager: Texture(Image) for loading is not valid!");

    std::vector<VkWriteDescriptorSet> writes;

    // Since on image creation index is UINT32_T::MAX
    outIndex = MakeOptional<uint32_t>(static_cast<uint32_t>(m_StorageImageIDPool.Add(m_StorageImageIDPool.GetSize())));
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               nullptr,
                                               m_MegaSet[frame],
                                               STORAGE_IMAGE_BINDING,
                                               outIndex.value(),
                                               1,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                               vkImageInfo,
                                               nullptr,
                                               nullptr};

        writes.emplace_back(writeSet);
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanDescriptorManager::LoadTexture(const void* pTextureInfo, Optional<uint32_t>& outIndex)
{
    const VkDescriptorImageInfo* vkTextureInfo = (const VkDescriptorImageInfo*)pTextureInfo;
    PFR_ASSERT(pTextureInfo && vkTextureInfo->imageView, "VulkanDescriptorManager: Texture(Image) for loading is not valid!");

    std::vector<VkWriteDescriptorSet> writes;

    // Since on image creation index is UINT32_T::MAX
    outIndex = MakeOptional<uint32_t>(static_cast<uint32_t>(m_TextureIDPool.Add(m_TextureIDPool.GetSize())));
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkWriteDescriptorSet writeSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    nullptr,       m_MegaSet[frame], TEXTURE_BINDING, outIndex.value(), 1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vkTextureInfo, nullptr,          nullptr};

        writes.emplace_back(writeSet);
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanDescriptorManager::FreeImage(Optional<uint32_t>& imageIndex)
{
    m_StorageImageIDPool.Release(imageIndex.value());
    imageIndex = std::nullopt;
}

void VulkanDescriptorManager::FreeTexture(Optional<uint32_t>& textureIndex)
{
    m_TextureIDPool.Release(textureIndex.value());
    textureIndex = std::nullopt;
}

void VulkanDescriptorManager::CreateDescriptorPools()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    {
        constexpr VkDescriptorBindingFlags bindingFlag =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        constexpr VkDescriptorSetLayoutBinding textureBinding = {TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES,
                                                                 VK_SHADER_STAGE_ALL};

        constexpr VkDescriptorSetLayoutBinding storageImageBinding = {STORAGE_IMAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES,
                                                                      VK_SHADER_STAGE_ALL};

        const std::vector<VkDescriptorSetLayoutBinding> bindings = {textureBinding, storageImageBinding};

        const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), bindingFlag);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo megaSetLayoutExtendedInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
            bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo megaSetLayoutCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &megaSetLayoutExtendedInfo,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, static_cast<uint32_t>(bindings.size()), bindings.data()};

        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &megaSetLayoutCI, nullptr, &m_MegaDescriptorSetLayout),
                 "Failed to create mega set layout!");
        VK_SetDebugName(logicalDevice, m_MegaDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_MEGA_SET_LAYOUT");
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const std::vector<VkDescriptorPoolSize> megaDescriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES},
                                                                           {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
        const VkDescriptorPoolCreateInfo megaDescriptorPoolCI           = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,         nullptr,
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,       1,
            static_cast<uint32_t>(megaDescriptorPoolSizes.size()), megaDescriptorPoolSizes.data()};

        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &megaDescriptorPoolCI, nullptr, &m_MegaDescriptorPool[frame]),
                 "Failed to create bindless mega descriptor pool!");
        const std::string megaDescriptorPoolPoolDebugName = std::string("VK_BINDLESS_MEGA_DESCRIPTOR_POOL_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, m_MegaDescriptorPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, megaDescriptorPoolPoolDebugName.data());

        const VkDescriptorSetAllocateInfo megaSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_MegaDescriptorPool[frame],
                                                       1, &m_MegaDescriptorSetLayout};
        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &megaSetAI, &m_MegaSet[frame]), "Failed to allocate mega set!");
        const std::string megaSetDebugName = std::string("VK_BINDLESS_MEGA_SET_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, m_MegaSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, megaSetDebugName.data());

        Renderer::GetStats().DescriptorSetCount += 1;
        Renderer::GetStats().DescriptorPoolCount += 1;
    }

    m_PCBlock.offset     = 0;
    m_PCBlock.stageFlags = VK_SHADER_STAGE_ALL;
    m_PCBlock.size       = sizeof(PushConstantBlock);
    PFR_ASSERT(m_PCBlock.size <= 128, "Exceeding minimum limit of push constant block!");

    const VkPipelineLayoutCreateInfo pipelineLayoutCI = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &m_MegaDescriptorSetLayout, 1, &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_MegaPipelineLayout),
             "Failed to create bindless pipeline layout!");
    VK_SetDebugName(logicalDevice, m_MegaPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VK_BINDLESS_PIPELINE_LAYOUT");
}

void VulkanDescriptorManager::Destroy()
{
    Renderer::GetStats().DescriptorSetCount -= s_FRAMES_IN_FLIGHT;
    Renderer::GetStats().DescriptorPoolCount -= s_FRAMES_IN_FLIGHT;
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    std::ranges::for_each(m_MegaDescriptorPool,
                          [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_MegaDescriptorSetLayout, nullptr);

    vkDestroyPipelineLayout(logicalDevice, m_MegaPipelineLayout, nullptr);

    LOG_INFO("Vulkan Descriptor Manager destroyed!");
}

}  // namespace Pathfinder