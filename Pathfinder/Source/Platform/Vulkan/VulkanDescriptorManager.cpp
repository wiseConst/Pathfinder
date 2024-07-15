#include <PathfinderPCH.h>
#include "VulkanDescriptorManager.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Renderer.h>

namespace Pathfinder
{

VulkanDescriptorManager::VulkanDescriptorManager()
{
    CreateDescriptorPools();
    LOG_INFO("{}", __FUNCTION__);
}

void VulkanDescriptorManager::Bind(const Shared<CommandBuffer>& commandBuffer, const RendererTypeFlags bindPoints)
{
    const auto currentFrame    = Application::Get().GetWindow()->GetCurrentFrameIndex();
    const auto& currentMegaSet = m_MegaSet[currentFrame];

    // NOTE: If override bind points aren't specified we use bindpoint based on command buffer type, otherwise we handle override bind
    // points.
    if (bindPoints == EPipelineStage::PIPELINE_STAGE_NONE)
    {
        const VkPipelineBindPoint pipelineBindPoint =
            commandBuffer->GetSpecification().Type == ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                                                                                      : VK_PIPELINE_BIND_POINT_COMPUTE;

        vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), pipelineBindPoint, m_MegaPipelineLayout, 0, 1, &currentMegaSet, 0,
                                nullptr);
        return;
    }

    bool bAnythingBound = false;
    if ((bindPoints & EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT) == EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT)
    {
        vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), VK_PIPELINE_BIND_POINT_COMPUTE, m_MegaPipelineLayout, 0, 1,
                                &currentMegaSet, 0, nullptr);
        bAnythingBound = true;
    }

    if ((bindPoints & EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT) == EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT)
    {
        vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_MegaPipelineLayout, 0, 1,
                                &currentMegaSet, 0, nullptr);
        bAnythingBound = true;
    }

    if ((bindPoints & EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT) == EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT)
    {
        vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_MegaPipelineLayout, 0, 1,
                                &currentMegaSet, 0, nullptr);
        bAnythingBound = true;
    }

    PFR_ASSERT(bAnythingBound, "Unknown pipeline stages specified to override pipeline bind point for descriptor sets!");
}

void VulkanDescriptorManager::LoadImage(const void* pImageInfo, Optional<uint32_t>& outIndex)
{
    const VkDescriptorImageInfo* vkImageInfo = (const VkDescriptorImageInfo*)pImageInfo;
    PFR_ASSERT(pImageInfo && vkImageInfo->imageView, "VulkanDescriptorManager: Texture(Image) for loading is not valid!");

    std::vector<VkWriteDescriptorSet> writes;
    std::scoped_lock lock(m_UploadMutex);

    // Since on image creation index is UINT32_T::MAX
    outIndex = MakeOptional<uint32_t>(static_cast<uint32_t>(m_StorageImageIDPool.Add(m_StorageImageIDPool.GetSize())));
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkWriteDescriptorSet writeSet = {.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .dstSet          = m_MegaSet[frame],
                                               .dstBinding      = STORAGE_IMAGE_BINDING,
                                               .dstArrayElement = outIndex.value(),
                                               .descriptorCount = 1,
                                               .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                               .pImageInfo      = vkImageInfo};

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
    std::scoped_lock lock(m_UploadMutex);

    // Since on image creation index is UINT32_T::MAX
    outIndex = MakeOptional<uint32_t>(static_cast<uint32_t>(m_TextureIDPool.Add(m_TextureIDPool.GetSize())));
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkWriteDescriptorSet writeSet = {.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .dstSet          = m_MegaSet[frame],
                                               .dstBinding      = TEXTURE_BINDING,
                                               .dstArrayElement = outIndex.value(),
                                               .descriptorCount = 1,
                                               .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               .pImageInfo      = vkTextureInfo};

        writes.emplace_back(writeSet);
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanDescriptorManager::CreateDescriptorPools()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    constexpr VkDescriptorBindingFlags bindingFlag        = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    constexpr VkDescriptorSetLayoutBinding textureBinding = {.binding         = TEXTURE_BINDING,
                                                             .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                             .descriptorCount = s_MAX_TEXTURES,
                                                             .stageFlags      = VK_SHADER_STAGE_ALL};

    constexpr VkDescriptorSetLayoutBinding storageImageBinding = {.binding         = STORAGE_IMAGE_BINDING,
                                                                  .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                                  .descriptorCount = s_MAX_IMAGES,
                                                                  .stageFlags      = VK_SHADER_STAGE_ALL};

    const std::vector<VkDescriptorSetLayoutBinding> bindings = {textureBinding, storageImageBinding};

    const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), bindingFlag);
    const VkDescriptorSetLayoutBindingFlagsCreateInfo megaSetLayoutExtendedInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount  = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data()};

    const VkDescriptorSetLayoutCreateInfo megaSetLayoutCI = {.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                             .pNext        = &megaSetLayoutExtendedInfo,
                                                             .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                             .bindingCount = static_cast<uint32_t>(bindings.size()),
                                                             .pBindings    = bindings.data()};

    VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &megaSetLayoutCI, nullptr, &m_MegaDescriptorSetLayout),
             "Failed to create mega set layout!");
    VK_SetDebugName(logicalDevice, m_MegaDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_MEGA_SET_LAYOUT");

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const std::vector<VkDescriptorPoolSize> megaDescriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES},
                                                                           {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
        const VkDescriptorPoolCreateInfo megaDescriptorPoolCI =
            VulkanUtils::GetDescriptorPoolCreateInfo(static_cast<uint32_t>(megaDescriptorPoolSizes.size()), 1,
                                                     megaDescriptorPoolSizes.data(), VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &megaDescriptorPoolCI, nullptr, &m_MegaDescriptorPool[frame]),
                 "Failed to create bindless mega descriptor pool!");
        const std::string megaDescriptorPoolPoolDebugName = std::string("VK_BINDLESS_MEGA_DESCRIPTOR_POOL_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, m_MegaDescriptorPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                        megaDescriptorPoolPoolDebugName.data());

        const VkDescriptorSetAllocateInfo megaSetAI =
            VulkanUtils::GetDescriptorSetAllocateInfo(m_MegaDescriptorPool[frame], 1, &m_MegaDescriptorSetLayout);
        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &megaSetAI, &m_MegaSet[frame]), "Failed to allocate mega set!");
        const std::string megaSetDebugName = std::string("VK_BINDLESS_MEGA_SET_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, m_MegaSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, megaSetDebugName.data());
    }

    m_PCBlock = VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_ALL, .offset = 0, .size = sizeof(PushConstantBlock)};
    PFR_ASSERT(m_PCBlock.size == 128, "Exceeding minimum limit of push constant block!");

    const VkPipelineLayoutCreateInfo pipelineLayoutCI = {.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         .setLayoutCount         = 1,
                                                         .pSetLayouts            = &m_MegaDescriptorSetLayout,
                                                         .pushConstantRangeCount = 1,
                                                         .pPushConstantRanges    = &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_MegaPipelineLayout),
             "Failed to create bindless pipeline layout!");
    VK_SetDebugName(logicalDevice, m_MegaPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VK_BINDLESS_PIPELINE_LAYOUT");
}

void VulkanDescriptorManager::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    std::ranges::for_each(m_MegaDescriptorPool,
                          [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_MegaDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, m_MegaPipelineLayout, nullptr);

    LOG_INFO("{}", __FUNCTION__);
}

}  // namespace Pathfinder