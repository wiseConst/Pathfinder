#include "PathfinderPCH.h"
#include "VulkanBindlessRenderer.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"

#include "Core/Application.h"
#include "Core/Window.h"

namespace Pathfinder
{

static constexpr uint32_t s_MAX_TEXTURES = BIT(16);
static constexpr uint32_t s_MAX_IMAGES   = BIT(16);
// TODO: Make bindless renderer somehow automated for creation from shader headers files

VulkanBindlessRenderer::VulkanBindlessRenderer()
{
    CreateDescriptorPools();
    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer created!");
}

void VulkanBindlessRenderer::Bind(const Shared<CommandBuffer>& commandBuffer)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();
    // vkCmdBindDescriptorSets((VkCommandBuffer)vulkanCommandBuffer->Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_Layout, 0, 1,
    // &m_TextureSet[currentFrame], 0, nullptr);
    vkCmdBindDescriptorSets((VkCommandBuffer)vulkanCommandBuffer->Get(), VK_PIPELINE_BIND_POINT_COMPUTE, m_Layout, 1, 1,
                            &m_ImageSet[currentFrame], 0, nullptr);
}

void VulkanBindlessRenderer::LoadImage(const ImagePerFrame& images)
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        auto vkImage = std::static_pointer_cast<VulkanImage>(images[frame]);

        if (!m_FreeImageIndicesPool.empty())
        {
            vkImage->m_Index = m_FreeImageIndicesPool.back();
            m_FreeImageIndicesPool.pop_back();
        }
        else
        {
            vkImage->m_Index = static_cast<uint32_t>(m_ImageIndicesPool.size());
            m_ImageIndicesPool.push_back(vkImage->m_Index);
        }

        const VkDescriptorImageInfo imageInfo = {nullptr, (VkImageView)vkImage->GetView(),
                                                 ImageUtils::PathfinderImageLayoutToVulkan(images[frame]->GetSpecification().Layout)};

        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_ImageSet[frame];
        writeSet.dstBinding           = 0;
        writeSet.pImageInfo           = &imageInfo;

        vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
    }
}

void VulkanBindlessRenderer::LoadImage(const Shared<Image>& image)
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    auto vkImage = std::static_pointer_cast<VulkanImage>(image);
    if (!m_FreeImageIndicesPool.empty())
    {
        vkImage->m_Index = m_FreeImageIndicesPool.back();
        m_FreeImageIndicesPool.pop_back();
    }
    else
    {
        vkImage->m_Index = static_cast<uint32_t>(m_ImageIndicesPool.size());
        m_ImageIndicesPool.push_back(vkImage->m_Index);
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkDescriptorImageInfo imageInfo = {nullptr, (VkImageView)vkImage->GetView(),
                                                 ImageUtils::PathfinderImageLayoutToVulkan(vkImage->GetSpecification().Layout)};

        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_ImageSet[frame];
        writeSet.dstBinding           = 0;
        writeSet.pImageInfo           = &imageInfo;

        vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
    }
}

void VulkanBindlessRenderer::FreeImage(uint32_t& imageIndex)
{
    m_FreeImageIndicesPool.push_back(imageIndex);
    imageIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::CreateDescriptorPools()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    constexpr std::array<VkDescriptorPoolSize, 1> texturePoolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES}};
    constexpr VkDescriptorSetLayoutBinding textureBinding          = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT};
    const VkDescriptorSetLayoutCreateInfo textureSetLayoutCI       = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
                                                                      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                      &textureBinding};
    VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &textureSetLayoutCI, nullptr, &m_TextureSetLayout),
             "Failed to create texture set layout!");

    constexpr std::array<VkDescriptorPoolSize, 1> imagePoolSizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
    constexpr VkDescriptorSetLayoutBinding imageBinding          = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT};
    const VkDescriptorSetLayoutCreateInfo imageSetLayoutCI       = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1, &imageBinding};
    VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &imageSetLayoutCI, nullptr, &m_ImageSetLayout),
             "Failed to create image set layout!");

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkDescriptorPoolCreateInfo texturePoolCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            /* VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | */ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            1,
            static_cast<uint32_t>(texturePoolSizes.size()),
            texturePoolSizes.data()};

        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &texturePoolCI, nullptr, &m_TexturePool[frame]),
                 "Failed to create bindless descriptor texture pool!");
        const std::string texturePoolDebugName = std::string("VK_BINDLESS_TEXTURE_POOL_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_TexturePool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, texturePoolDebugName.data());

        const VkDescriptorSetAllocateInfo textureDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                    m_TexturePool[frame], 1, &m_TextureSetLayout};

        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &textureDescriptorSetAI, &m_TextureSet[frame]), "Failed to allocate texture set!");

        const VkDescriptorPoolCreateInfo imagePoolCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            /* VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | */ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            1,
            static_cast<uint32_t>(imagePoolSizes.size()),
            imagePoolSizes.data()};

        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &imagePoolCI, nullptr, &m_ImagePool[frame]),
                 "Failed to create bindless descriptor image pool!");
        const std::string imagePoolDebugName = std::string("VK_BINDLESS_IMAGE_POOL_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_ImagePool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, imagePoolDebugName.data());

        const VkDescriptorSetAllocateInfo imageDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                  m_ImagePool[frame], 1, &m_ImageSetLayout};

        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &imageDescriptorSetAI, &m_ImageSet[frame]), "Failed to allocate image set!");
    }

    // layout useless for now

    m_PCBlock.offset     = 0;
    m_PCBlock.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    m_PCBlock.size       = sizeof(uint32_t) * 2;

    std::vector<VkDescriptorSetLayout> setLayouts     = {m_TextureSetLayout, m_ImageSetLayout};
    const VkPipelineLayoutCreateInfo pipelineLayoutCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         0,
                                                         static_cast<uint32_t>(setLayouts.size()),
                                                         setLayouts.data(),
                                                         1,
                                                         &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_Layout), "Failed to create bindless pipeline layout!");
}

void VulkanBindlessRenderer::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    std::ranges::for_each(m_TexturePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_TextureSetLayout, nullptr);

    std::ranges::for_each(m_ImagePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_ImageSetLayout, nullptr);

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);

    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer destroyed!");
}

}  // namespace Pathfinder