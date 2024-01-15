#include "PathfinderPCH.h"
#include "VulkanBindlessRenderer.h"

#include "VulkanBuffer.h"
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

void VulkanBindlessRenderer::Bind(const Shared<CommandBuffer>& commandBuffer, const bool bGraphicsBindPoint)
{
    const auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();

    VkDescriptorSet sets[] = {m_TextureSet[currentFrame], m_ImageSet[currentFrame], m_CameraSet[currentFrame]};
    vkCmdBindDescriptorSets((VkCommandBuffer)vulkanCommandBuffer->Get(),
                            bGraphicsBindPoint ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE, m_Layout, 0, 3, sets, 0,
                            nullptr);
}

void VulkanBindlessRenderer::UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer)
{
    const auto currentFrame   = Application::Get().GetWindow()->GetCurrentFrameIndex();
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    const auto vulkanCameraUniformBuffer = std::static_pointer_cast<VulkanBuffer>(cameraUniformBuffer);
    PFR_ASSERT(vulkanCameraUniformBuffer, "Failed to cast Buffer to VulkanBuffer!");

    VkWriteDescriptorSet cameraWriteSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    cameraWriteSet.descriptorCount      = 1;
    cameraWriteSet.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWriteSet.dstBinding           = 0;
    cameraWriteSet.dstSet               = m_CameraSet[currentFrame];
    cameraWriteSet.pBufferInfo          = &vulkanCameraUniformBuffer->GetDescriptorInfo();

    vkUpdateDescriptorSets(logicalDevice, 1, &cameraWriteSet, 0, nullptr);
}

void VulkanBindlessRenderer::LoadImage(const ImagePerFrame& images)
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        auto vulkanImage = std::static_pointer_cast<VulkanImage>(images[frame]);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        if (!m_FreeImageIndicesPool.empty())
        {
            vulkanImage->m_Index = m_FreeImageIndicesPool.back();
            m_FreeImageIndicesPool.pop_back();
        }
        else
        {
            vulkanImage->m_Index = static_cast<uint32_t>(m_ImageIndicesPool.size());
            m_ImageIndicesPool.push_back(vulkanImage->m_Index);
        }

        const VkDescriptorImageInfo imageInfo = {nullptr, (VkImageView)vulkanImage->GetView(),
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

    auto vulkanImage = std::static_pointer_cast<VulkanImage>(image);
    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

    if (!m_FreeImageIndicesPool.empty())
    {
        vulkanImage->m_Index = m_FreeImageIndicesPool.back();
        m_FreeImageIndicesPool.pop_back();
    }
    else
    {
        vulkanImage->m_Index = static_cast<uint32_t>(m_ImageIndicesPool.size());
        m_ImageIndicesPool.push_back(vulkanImage->m_Index);
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const VkDescriptorImageInfo imageInfo = {nullptr, (VkImageView)vulkanImage->GetView(),
                                                 ImageUtils::PathfinderImageLayoutToVulkan(vulkanImage->GetSpecification().Layout)};

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
    if (auto it = std::find(m_ImageIndicesPool.begin(), m_ImageIndicesPool.end(), imageIndex); it != m_ImageIndicesPool.end())
        m_ImageIndicesPool.erase(it);

    m_FreeImageIndicesPool.push_back(imageIndex);
    imageIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::CreateDescriptorPools()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    constexpr VkDescriptorBindingFlags bindingFlag =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT, nullptr, 1, &bindingFlag};

    {
        const VkDescriptorSetLayoutBinding textureBinding        = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                        VK_SHADER_STAGE_VERTEX_BIT};
        const VkDescriptorSetLayoutCreateInfo textureSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                    &textureBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &textureSetLayoutCI, nullptr, &m_TextureSetLayout),
                 "Failed to create texture set layout!");
        VK_SetDebugName(logicalDevice, (uint64_t)m_TextureSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "VK_BINDLESS_TEXTURE_SET_LAYOUT");
    }

    {
        const VkDescriptorSetLayoutBinding imageBinding        = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES,
                                                                  VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                      VK_SHADER_STAGE_VERTEX_BIT};
        const VkDescriptorSetLayoutCreateInfo imageSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                  &imageBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &imageSetLayoutCI, nullptr, &m_ImageSetLayout),
                 "Failed to create image set layout!");
        VK_SetDebugName(logicalDevice, (uint64_t)m_ImageSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_IMAGE_SET_LAYOUT");
    }

    {
        const VkDescriptorSetLayoutBinding cameraBinding        = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                                   VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                       VK_SHADER_STAGE_VERTEX_BIT};
        const VkDescriptorSetLayoutCreateInfo cameraSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                   &cameraBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &cameraSetLayoutCI, nullptr, &m_CameraSetLayout),
                 "Failed to create camera set layout!");
        VK_SetDebugName(logicalDevice, (uint64_t)m_CameraSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_CAMERA_SET_LAYOUT");
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        // TEXTURES
        constexpr std::array<VkDescriptorPoolSize, 1> texturePoolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES}};
        const VkDescriptorPoolCreateInfo texturePoolCI                 = {
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
        const std::string textureSetDebugName = std::string("VK_BINDLESS_TEXTURE_SET_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_TextureSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, textureSetDebugName.data());

        // STORAGE IMAGES
        constexpr std::array<VkDescriptorPoolSize, 1> imagePoolSizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
        const VkDescriptorPoolCreateInfo imagePoolCI                 = {
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
        const std::string imageSetDebugName = std::string("VK_BINDLESS_IMAGE_SET_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_ImageSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, imageSetDebugName.data());

        // CAMERA SET
        constexpr std::array<VkDescriptorPoolSize, 1> cameraPoolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
        const VkDescriptorPoolCreateInfo cameraPoolCI                 = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            /* VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | */ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            1,
            static_cast<uint32_t>(cameraPoolSizes.size()),
            cameraPoolSizes.data()};

        VK_CHECK(vkCreateDescriptorPool(logicalDevice, &cameraPoolCI, nullptr, &m_CameraPool[frame]),
                 "Failed to create bindless descriptor image pool!");
        const std::string cameraPoolDebugName = std::string("VK_BINDLESS_IMAGE_POOL_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_CameraPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, cameraPoolDebugName.data());

        const VkDescriptorSetAllocateInfo cameraDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                   m_CameraPool[frame], 1, &m_CameraSetLayout};
        VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &cameraDescriptorSetAI, &m_CameraSet[frame]), "Failed to allocate camera set!");
        const std::string cameraSetDebugName = std::string("VK_BINDLESS_IMAGE_SET_") + std::to_string(frame);
        VK_SetDebugName(logicalDevice, (uint64_t)m_CameraSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, cameraSetDebugName.data());
    }

    // layout useless for now

    m_PCBlock.offset     = 0;
    m_PCBlock.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    m_PCBlock.size       = sizeof(uint32_t) * 2 + sizeof(glm::mat4);

    std::vector<VkDescriptorSetLayout> setLayouts     = {m_TextureSetLayout, m_ImageSetLayout, m_CameraSetLayout};
    const VkPipelineLayoutCreateInfo pipelineLayoutCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                         nullptr,
                                                         0,
                                                         static_cast<uint32_t>(setLayouts.size()),
                                                         setLayouts.data(),
                                                         1,
                                                         &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_Layout), "Failed to create bindless pipeline layout!");
    VK_SetDebugName(logicalDevice, (uint64_t)m_Layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VK_BINDLESS_PIPELINE_LAYOUT");
}

void VulkanBindlessRenderer::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    std::ranges::for_each(m_TexturePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_TextureSetLayout, nullptr);

    std::ranges::for_each(m_ImagePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_ImageSetLayout, nullptr);

    std::ranges::for_each(m_CameraPool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_CameraSetLayout, nullptr);

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);

    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer destroyed!");
}

}  // namespace Pathfinder