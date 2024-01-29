#include "PathfinderPCH.h"
#include "VulkanBindlessRenderer.h"

#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanImage.h"
#include "VulkanTexture2D.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "Renderer/Renderer.h"

namespace Pathfinder
{

// TODO: Make bindless renderer somehow automated for creation from shader headers files
static constexpr uint32_t s_MAX_TEXTURES        = BIT(16);
static constexpr uint32_t s_MAX_IMAGES          = BIT(16);
static constexpr uint32_t s_MAX_STORAGE_BUFFERS = BIT(16);

// TODO: Add shader defines for each binding and set
VulkanBindlessRenderer::VulkanBindlessRenderer()
{
    CreateDescriptorPools();
    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer created!");
}

void VulkanBindlessRenderer::Bind(const Shared<CommandBuffer>& commandBuffer)
{
    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();

    const auto pipelineBindPoint = commandBuffer->GetType() == ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS
                                       ? VK_PIPELINE_BIND_POINT_GRAPHICS
                                       : VK_PIPELINE_BIND_POINT_COMPUTE;
    VkDescriptorSet sets[]       = {m_TextureSet[currentFrame], m_ImageSet[currentFrame], m_StorageBufferSet[currentFrame],
                                    m_CameraSet[currentFrame]};
    vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), pipelineBindPoint, m_Layout, 0, 4, sets, 0, nullptr);
}

void VulkanBindlessRenderer::UpdateDataIfNeeded()
{
    if (m_Writes.empty()) return;

    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(m_Writes.size()), m_Writes.data(), 0,
                           nullptr);

    m_Writes.clear();
}

void VulkanBindlessRenderer::UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer)
{
    const auto vulkanCameraUniformBuffer = std::static_pointer_cast<VulkanBuffer>(cameraUniformBuffer);
    PFR_ASSERT(vulkanCameraUniformBuffer, "Failed to cast Buffer to VulkanBuffer!");

    const auto currentFrame             = Application::Get().GetWindow()->GetCurrentFrameIndex();
    VkWriteDescriptorSet cameraWriteSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    cameraWriteSet.descriptorCount      = 1;
    cameraWriteSet.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWriteSet.dstBinding           = 0;
    cameraWriteSet.dstSet               = m_CameraSet[currentFrame];
    cameraWriteSet.pBufferInfo          = &vulkanCameraUniformBuffer->GetDescriptorInfo();
    m_Writes.emplace_back(cameraWriteSet);
}

void VulkanBindlessRenderer::LoadImage(const ImagePerFrame& images)
{
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

        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_ImageSet[frame];
        writeSet.dstBinding           = 0;
        writeSet.dstArrayElement      = vulkanImage->m_Index;
        writeSet.pImageInfo           = &vulkanImage->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::LoadImage(const Shared<Image>& image)
{
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
        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_ImageSet[frame];
        writeSet.dstBinding           = 0;
        writeSet.dstArrayElement      = vulkanImage->m_Index;
        writeSet.pImageInfo           = &vulkanImage->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::LoadTexture(const Shared<Texture2D>& texture)
{
    auto vulkanTexture = std::static_pointer_cast<VulkanTexture2D>(texture);
    PFR_ASSERT(vulkanTexture, "Failed to cast Texture2D to VulkanTexture2D!");

    if (m_FreeTextureIndicesPool.empty())
    {
        vulkanTexture->m_Index = m_TextureIndicesPool.size();
        m_TextureIndicesPool.emplace_back(vulkanTexture->m_Index);
    }
    else
    {
        vulkanTexture->m_Index = m_FreeTextureIndicesPool.back();
        m_FreeTextureIndicesPool.pop_back();
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSet.descriptorCount      = 1;
        writeSet.dstSet               = m_TextureSet[frame];
        writeSet.dstBinding           = 0;
        writeSet.dstArrayElement      = vulkanTexture->m_Index;
        writeSet.pImageInfo           = &vulkanTexture->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::LoadVertexPosBuffer(const Shared<Buffer>& buffer)
{
    LoadStorageBufferInternal(buffer, STORAGE_BUFFER_VERTEX_POS_BINDING);
}

void VulkanBindlessRenderer::LoadVertexAttribBuffer(const Shared<Buffer>& buffer)
{
    LoadStorageBufferInternal(buffer, STORAGE_BUFFER_VERTEX_ATTRIB_BINDING);
}

void VulkanBindlessRenderer::LoadMeshletBuffer(const Shared<Buffer>& buffer)
{
    LoadStorageBufferInternal(buffer, STORAGE_BUFFER_MESHLET_BINDING);
}

void VulkanBindlessRenderer::LoadMeshletVerticesBuffer(const Shared<Buffer>& buffer)
{
    LoadStorageBufferInternal(buffer, STORAGE_BUFFER_MESHLET_VERTEX_BINDING);
}

void VulkanBindlessRenderer::LoadMeshletTrianglesBuffer(const Shared<Buffer>& buffer)
{
    LoadStorageBufferInternal(buffer, STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING);
}

void VulkanBindlessRenderer::LoadStorageBufferInternal(const Shared<Buffer>& buffer, const uint32_t binding)
{
    auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    if (!m_FreeStorageBufferIndicesPool.empty())
    {
        vulkanBuffer->m_Index = m_FreeStorageBufferIndicesPool.back();
        m_FreeStorageBufferIndicesPool.pop_back();
    }
    else
    {
        vulkanBuffer->m_Index = static_cast<uint32_t>(m_StorageBufferIndicesPool.size());
        m_StorageBufferIndicesPool.push_back(vulkanBuffer->m_Index);
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeSet.dstSet               = m_StorageBufferSet[frame];
        writeSet.dstBinding           = binding;
        writeSet.dstArrayElement      = vulkanBuffer->m_Index;
        writeSet.pBufferInfo          = &vulkanBuffer->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::FreeImage(uint32_t& imageIndex)
{
    m_FreeImageIndicesPool.emplace_back(imageIndex);
    imageIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::FreeBuffer(uint32_t& bufferIndex)
{
    m_FreeStorageBufferIndicesPool.emplace_back(bufferIndex);
    bufferIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::FreeTexture(uint32_t& textureIndex)
{
    m_FreeTextureIndicesPool.emplace_back(textureIndex);
    textureIndex = UINT32_MAX;
}

// TODO: Clean it, remove redundant shader stages
void VulkanBindlessRenderer::CreateDescriptorPools()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    // BINDLESS FLAGS
    constexpr VkDescriptorBindingFlags bindingFlag =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                                      nullptr, 1, &bindingFlag};

    // TEXTURES
    {
        const VkDescriptorSetLayoutBinding textureBinding        = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                        VK_SHADER_STAGE_VERTEX_BIT};
        const VkDescriptorSetLayoutCreateInfo textureSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                    VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                    &textureBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &textureSetLayoutCI, nullptr, &m_TextureSetLayout),
                 "Failed to create texture set layout!");
        VK_SetDebugName(logicalDevice, &m_TextureSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_TEXTURE_SET_LAYOUT");
    }

    // STORAGE IMAGES
    {
        const VkDescriptorSetLayoutBinding imageBinding        = {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES,
                                                                  VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                      VK_SHADER_STAGE_VERTEX_BIT};
        const VkDescriptorSetLayoutCreateInfo imageSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                  &imageBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &imageSetLayoutCI, nullptr, &m_ImageSetLayout),
                 "Failed to create image set layout!");
        VK_SetDebugName(logicalDevice, &m_ImageSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_IMAGE_SET_LAYOUT");
    }

    // STORAGE BUFFERS
    {
        const VkDescriptorSetLayoutBinding vertexPosBufferBinding = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
                                                                     VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding vertexAttribBufferBinding = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
                                                                        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletBufferBinding = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
                                                                   VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletVerticesBufferBinding = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
                                                                           VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletTrianglesBufferBinding = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
                                                                            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const std::vector<VkDescriptorBindingFlags> bindingFlags(5, bindingFlag);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo bufferExtendedInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
            bindingFlags.data()};

        const std::vector<VkDescriptorSetLayoutBinding> bindings = {vertexPosBufferBinding, vertexAttribBufferBinding, meshletBufferBinding,
                                                                    meshletVerticesBufferBinding, meshletTrianglesBufferBinding};
        const VkDescriptorSetLayoutCreateInfo storageBufferSetLayoutCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &bufferExtendedInfo,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, static_cast<uint32_t>(bindings.size()), bindings.data()};

        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &storageBufferSetLayoutCI, nullptr, &m_StorageBufferSetLayout),
                 "Failed to create storage buffer set layout!");
        VK_SetDebugName(logicalDevice, &m_StorageBufferSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "VK_BINDLESS_STORAGE_BUFFER_SET_LAYOUT");
    }

    // CAMERA SET
    {
        VkDescriptorSetLayoutBinding cameraBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                          VK_SHADER_STAGE_VERTEX_BIT};
        if (Renderer::GetRendererSettings().bMeshShadingSupport) cameraBinding.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;

        const VkDescriptorSetLayoutCreateInfo cameraSetLayoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &extendedInfo,
                                                                   VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, 1,
                                                                   &cameraBinding};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &cameraSetLayoutCI, nullptr, &m_CameraSetLayout),
                 "Failed to create camera set layout!");
        VK_SetDebugName(logicalDevice, &m_CameraSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_CAMERA_SET_LAYOUT");
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        {
            // TEXTURES
            constexpr std::array<VkDescriptorPoolSize, 1> texturePoolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES}};
            const VkDescriptorPoolCreateInfo texturePoolCI                 = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,   nullptr,
                                                                              VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 1,
                                                                              static_cast<uint32_t>(texturePoolSizes.size()),  texturePoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &texturePoolCI, nullptr, &m_TexturePool[frame]),
                     "Failed to create bindless descriptor texture pool!");
            const std::string texturePoolDebugName = std::string("VK_BINDLESS_TEXTURE_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_TexturePool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, texturePoolDebugName.data());

            const VkDescriptorSetAllocateInfo textureDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                        m_TexturePool[frame], 1, &m_TextureSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &textureDescriptorSetAI, &m_TextureSet[frame]),
                     "Failed to allocate texture set!");
            const std::string textureSetDebugName = std::string("VK_BINDLESS_TEXTURE_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_TextureSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, textureSetDebugName.data());
        }

        {
            // STORAGE IMAGES
            constexpr std::array<VkDescriptorPoolSize, 1> imagePoolSizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
            const VkDescriptorPoolCreateInfo imagePoolCI                 = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,   nullptr,
                                                                            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 1,
                                                                            static_cast<uint32_t>(imagePoolSizes.size()),    imagePoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &imagePoolCI, nullptr, &m_ImagePool[frame]),
                     "Failed to create bindless descriptor image pool!");
            const std::string imagePoolDebugName = std::string("VK_BINDLESS_IMAGE_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_ImagePool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, imagePoolDebugName.data());

            const VkDescriptorSetAllocateInfo imageDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                      m_ImagePool[frame], 1, &m_ImageSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &imageDescriptorSetAI, &m_ImageSet[frame]), "Failed to allocate image set!");
            const std::string imageSetDebugName = std::string("VK_BINDLESS_IMAGE_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_ImageSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, imageSetDebugName.data());
        }

        {
            // STORAGE BUFFERS
            constexpr std::array<VkDescriptorPoolSize, 1> storageBufferPoolSizes = {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_STORAGE_BUFFERS}};
            const VkDescriptorPoolCreateInfo storageBufferPoolCI = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,        nullptr,
                VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,      1,
                static_cast<uint32_t>(storageBufferPoolSizes.size()), storageBufferPoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &storageBufferPoolCI, nullptr, &m_StorageBufferPool[frame]),
                     "Failed to create bindless descriptor storage buffer pool!");
            const std::string storageBufferPoolDebugName = std::string("VK_BINDLESS_STORAGE_BUFFER_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_StorageBufferPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, storageBufferPoolDebugName.data());

            const VkDescriptorSetAllocateInfo storageBufferDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                              m_StorageBufferPool[frame], 1, &m_StorageBufferSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &storageBufferDescriptorSetAI, &m_StorageBufferSet[frame]),
                     "Failed to allocate storage buffer set!");
            const std::string storageBufferSetDebugName = std::string("VK_BINDLESS_STORAGE_BUFFER_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_StorageBufferSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, storageBufferSetDebugName.data());
        }

        {
            // CAMERA SET
            constexpr std::array<VkDescriptorPoolSize, 1> cameraPoolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
            const VkDescriptorPoolCreateInfo cameraPoolCI                 = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,   nullptr,
                                                                             VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, 1,
                                                                             static_cast<uint32_t>(cameraPoolSizes.size()),   cameraPoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &cameraPoolCI, nullptr, &m_CameraPool[frame]),
                     "Failed to create bindless descriptor image pool!");
            const std::string cameraPoolDebugName = std::string("VK_BINDLESS_IMAGE_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_CameraPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, cameraPoolDebugName.data());

            const VkDescriptorSetAllocateInfo cameraDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                       m_CameraPool[frame], 1, &m_CameraSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &cameraDescriptorSetAI, &m_CameraSet[frame]),
                     "Failed to allocate camera set!");
            const std::string cameraSetDebugName = std::string("VK_BINDLESS_IMAGE_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, &m_CameraSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, cameraSetDebugName.data());
        }

        Renderer::GetStats().DescriptorSetCount += 4;
        Renderer::GetStats().DescriptorPoolCount += 4;
    }

    m_PCBlock.offset     = 0;
    m_PCBlock.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (Renderer::GetRendererSettings().bMeshShadingSupport) m_PCBlock.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
    m_PCBlock.size = sizeof(PushConstantBlock);
    PFR_ASSERT(m_PCBlock.size <= 128, "Exceeding minimum limit of push constant block!");

    const std::vector<VkDescriptorSetLayout> setLayouts = {m_TextureSetLayout, m_ImageSetLayout, m_StorageBufferSetLayout,
                                                           m_CameraSetLayout};
    const VkPipelineLayoutCreateInfo pipelineLayoutCI   = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0,
                                                           static_cast<uint32_t>(setLayouts.size()),
                                                           setLayouts.data(),
                                                           1,
                                                           &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_Layout), "Failed to create bindless pipeline layout!");
    VK_SetDebugName(logicalDevice, &m_Layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VK_BINDLESS_PIPELINE_LAYOUT");
}

void VulkanBindlessRenderer::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    Renderer::GetStats().DescriptorSetCount += 4;
    Renderer::GetStats().DescriptorPoolCount += 4;

    std::ranges::for_each(m_TexturePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_TextureSetLayout, nullptr);

    std::ranges::for_each(m_ImagePool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_ImageSetLayout, nullptr);

    std::ranges::for_each(m_StorageBufferPool,
                          [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_StorageBufferSetLayout, nullptr);

    std::ranges::for_each(m_CameraPool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_CameraSetLayout, nullptr);

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);

    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer destroyed!");
}

}  // namespace Pathfinder