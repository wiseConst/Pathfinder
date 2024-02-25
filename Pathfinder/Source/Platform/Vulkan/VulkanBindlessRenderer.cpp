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

void VulkanBindlessRenderer::Bind(const Shared<CommandBuffer>& commandBuffer, const EPipelineStage overrideBindPoint)
{
    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();

    VkPipelineBindPoint pipelineBindPoint = commandBuffer->GetType() == ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS
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

    const std::vector<VkDescriptorSet> sets = {m_TextureStorageImageSet[currentFrame], m_StorageBufferSet[currentFrame],
                                               m_FrameDataSet[currentFrame]};
    vkCmdBindDescriptorSets((VkCommandBuffer)commandBuffer->Get(), pipelineBindPoint, m_Layout, 0, static_cast<uint32_t>(sets.size()),
                            sets.data(), 0, nullptr);
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
    cameraWriteSet.dstBinding           = FRAME_DATA_BUFFER_CAMERA_BINDING;
    cameraWriteSet.dstSet               = m_FrameDataSet[currentFrame];
    cameraWriteSet.pBufferInfo          = &vulkanCameraUniformBuffer->GetDescriptorInfo();
    m_Writes.emplace_back(cameraWriteSet);
}

void VulkanBindlessRenderer::UpdateLightData(const Shared<Buffer>& lightDataUniformBuffer)
{
    const auto vulkanLightDataUniformBuffer = std::static_pointer_cast<VulkanBuffer>(lightDataUniformBuffer);
    PFR_ASSERT(vulkanLightDataUniformBuffer, "Failed to cast Buffer to VulkanBuffer!");

    const auto currentFrame             = Application::Get().GetWindow()->GetCurrentFrameIndex();
    VkWriteDescriptorSet cameraWriteSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    cameraWriteSet.descriptorCount      = 1;
    cameraWriteSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cameraWriteSet.dstBinding           = FRAME_DATA_BUFFER_LIGHTS_BINDING;
    cameraWriteSet.dstSet               = m_FrameDataSet[currentFrame];
    cameraWriteSet.pBufferInfo          = &vulkanLightDataUniformBuffer->GetDescriptorInfo();
    m_Writes.emplace_back(cameraWriteSet);
}

void VulkanBindlessRenderer::LoadImage(const ImagePerFrame& images)
{
    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        auto vulkanImage = std::static_pointer_cast<VulkanImage>(images[frame]);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        if (!m_StorageImagePool.Free.empty())
        {
            vulkanImage->m_Index = m_StorageImagePool.Free.back();
            m_StorageImagePool.Free.pop_back();
        }
        else
        {
            vulkanImage->m_Index = static_cast<uint32_t>(m_StorageImagePool.Busy.size());
            m_StorageImagePool.Busy.push_back(vulkanImage->m_Index);
        }

        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_TextureStorageImageSet[frame];
        writeSet.dstBinding           = STORAGE_IMAGE_BINDING;
        writeSet.dstArrayElement      = vulkanImage->m_Index;
        writeSet.pImageInfo           = &vulkanImage->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::LoadImage(const Shared<Image>& image)
{
    auto vulkanImage = std::static_pointer_cast<VulkanImage>(image);
    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

    if (!m_StorageImagePool.Free.empty())
    {
        vulkanImage->m_Index = m_StorageImagePool.Free.back();
        m_StorageImagePool.Free.pop_back();
    }
    else
    {
        vulkanImage->m_Index = static_cast<uint32_t>(m_StorageImagePool.Busy.size());
        m_StorageImagePool.Busy.push_back(vulkanImage->m_Index);
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorCount      = 1;
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstSet               = m_TextureStorageImageSet[frame];
        writeSet.dstBinding           = STORAGE_IMAGE_BINDING;
        writeSet.dstArrayElement      = vulkanImage->m_Index;
        writeSet.pImageInfo           = &vulkanImage->GetDescriptorInfo();
        m_Writes.emplace_back(writeSet);
    }
}

void VulkanBindlessRenderer::LoadTexture(const Shared<Texture2D>& texture)
{
    auto vulkanTexture = std::static_pointer_cast<VulkanTexture2D>(texture);
    PFR_ASSERT(vulkanTexture, "Failed to cast Texture2D to VulkanTexture2D!");

    if (!m_TexturePool.Free.empty())
    {
        vulkanTexture->m_Index = m_TexturePool.Free.back();
        m_TexturePool.Free.pop_back();
    }
    else
    {
        vulkanTexture->m_Index = static_cast<uint32_t>(m_TexturePool.Busy.size());
        m_TexturePool.Busy.push_back(vulkanTexture->m_Index);
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        VkWriteDescriptorSet writeSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSet.descriptorCount      = 1;
        writeSet.dstSet               = m_TextureStorageImageSet[frame];
        writeSet.dstBinding           = TEXTURE_BINDING;
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

    if (!m_StorageBufferIndicesPool[binding].Free.empty())
    {
        vulkanBuffer->m_Index = m_StorageBufferIndicesPool[binding].Free.back();
        m_StorageBufferIndicesPool[binding].Free.pop_back();
    }
    else
    {
        vulkanBuffer->m_Index = static_cast<uint32_t>(m_StorageBufferIndicesPool[binding].Busy.size());
        m_StorageBufferIndicesPool[binding].Busy.push_back(vulkanBuffer->m_Index);
    }

    vulkanBuffer->m_BufferBinding = binding;
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
    m_StorageImagePool.Free.emplace_back(imageIndex);
    imageIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::FreeBuffer(uint32_t& bufferIndex, uint32_t bufferBinding)
{
    m_StorageBufferIndicesPool[bufferBinding].Free.emplace_back(bufferIndex);
    bufferIndex = UINT32_MAX;
}

void VulkanBindlessRenderer::FreeTexture(uint32_t& textureIndex)
{
    m_TexturePool.Free.emplace_back(textureIndex);
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

    // TEXTURES + STORAGE IMAGES
    {
        const VkDescriptorSetLayoutBinding textureBinding = {TEXTURE_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES,
                                                             VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                 VK_SHADER_STAGE_VERTEX_BIT};

        const VkDescriptorSetLayoutBinding storageImageBinding = {STORAGE_IMAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES,
                                                                  VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                                      VK_SHADER_STAGE_VERTEX_BIT};

        const std::vector<VkDescriptorSetLayoutBinding> bindings = {textureBinding, storageImageBinding};

        const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), bindingFlag);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo textureStorageImageExtendedInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
            bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo textureStorageImageSetLayoutCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &textureStorageImageExtendedInfo,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, static_cast<uint32_t>(bindings.size()), bindings.data()};

        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &textureStorageImageSetLayoutCI, nullptr, &m_TextureStorageImageSetLayout),
                 "Failed to create texture + storage image set layout!");
        VK_SetDebugName(logicalDevice, m_TextureStorageImageSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "VK_BINDLESS_TEXTURE_STORAGE_IMAGE_SET_LAYOUT");
    }

    // STORAGE BUFFERS
    {
        const VkDescriptorSetLayoutBinding vertexPosBufferBinding = {
            STORAGE_BUFFER_VERTEX_POS_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding vertexAttribBufferBinding = {
            STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletBufferBinding = {
            STORAGE_BUFFER_MESHLET_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT |
                VK_SHADER_STAGE_TASK_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletVerticesBufferBinding = {
            STORAGE_BUFFER_MESHLET_VERTEX_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const VkDescriptorSetLayoutBinding meshletTrianglesBufferBinding = {
            STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, s_MAX_STORAGE_BUFFERS,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT};

        const std::vector<VkDescriptorSetLayoutBinding> bindings = {vertexPosBufferBinding, vertexAttribBufferBinding, meshletBufferBinding,
                                                                    meshletVerticesBufferBinding, meshletTrianglesBufferBinding};

        const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), bindingFlag);
        const VkDescriptorSetLayoutBindingFlagsCreateInfo storageBufferExtendedInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
            bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo storageBufferSetLayoutCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &storageBufferExtendedInfo,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, static_cast<uint32_t>(bindings.size()), bindings.data()};

        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &storageBufferSetLayoutCI, nullptr, &m_StorageBufferSetLayout),
                 "Failed to create storage buffer set layout!");
        VK_SetDebugName(logicalDevice, m_StorageBufferSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                        "VK_BINDLESS_STORAGE_BUFFER_SET_LAYOUT");
    }

    // FRAME DATA BUFFER SET
    {
        VkDescriptorSetLayoutBinding cameraBinding    = {FRAME_DATA_BUFFER_CAMERA_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                             VK_SHADER_STAGE_VERTEX_BIT};
        VkDescriptorSetLayoutBinding lightDataBinding = {FRAME_DATA_BUFFER_LIGHTS_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT |
                                                             VK_SHADER_STAGE_VERTEX_BIT};
        if (Renderer::GetRendererSettings().bMeshShadingSupport)
        {
            cameraBinding.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;
            lightDataBinding.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
        }

        const std::vector<VkDescriptorSetLayoutBinding> bindings = {cameraBinding, lightDataBinding};
        const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), bindingFlag);
        VkDescriptorSetLayoutBindingFlagsCreateInfo frameDataExtendedInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
            bindingFlags.data()};

        const VkDescriptorSetLayoutCreateInfo frameDataSetLayoutCI = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &frameDataExtendedInfo,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, static_cast<uint32_t>(bindings.size()), bindings.data()};
        VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &frameDataSetLayoutCI, nullptr, &m_FrameDataSetLayout),
                 "Failed to create frame data set layout!");
        VK_SetDebugName(logicalDevice, m_FrameDataSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VK_BINDLESS_FRAME_DATA_SET_LAYOUT");
    }

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        {
            // TEXTURES + STORAGE IMAGES
            const std::vector<VkDescriptorPoolSize> textureStorageImagePoolSizes = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s_MAX_TEXTURES}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, s_MAX_IMAGES}};
            const VkDescriptorPoolCreateInfo textureStorageImagePoolCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                                          nullptr,
                                                                          VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                                                          1,
                                                                          static_cast<uint32_t>(textureStorageImagePoolSizes.size()),
                                                                          textureStorageImagePoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &textureStorageImagePoolCI, nullptr, &m_TextureStorageImagePool[frame]),
                     "Failed to create bindless descriptor texture + storage image pool!");
            const std::string textureStorageImagePoolDebugName =
                std::string("VK_BINDLESS_TEXTURE_STORAGE_IMAGE_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_TextureStorageImagePool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                            textureStorageImagePoolDebugName.data());

            const VkDescriptorSetAllocateInfo textureStorageImageDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                                    m_TextureStorageImagePool[frame], 1,
                                                                                    &m_TextureStorageImageSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &textureStorageImageDescriptorSetAI, &m_TextureStorageImageSet[frame]),
                     "Failed to allocate texture + storage image set!");
            const std::string textureStorageImageSetDebugName =
                std::string("VK_BINDLESS_TEXTURE_STORAGE_IMAGE_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_TextureStorageImageSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET,
                            textureStorageImageSetDebugName.data());
        }

        {
            // STORAGE BUFFERS
            constexpr std::array<VkDescriptorPoolSize, 1> storageBufferPoolSizes = {
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * s_MAX_STORAGE_BUFFERS}};
            const VkDescriptorPoolCreateInfo storageBufferPoolCI = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,        nullptr,
                VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,      1,
                static_cast<uint32_t>(storageBufferPoolSizes.size()), storageBufferPoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &storageBufferPoolCI, nullptr, &m_StorageBufferPool[frame]),
                     "Failed to create bindless descriptor storage buffer pool!");
            const std::string storageBufferPoolDebugName = std::string("VK_BINDLESS_STORAGE_BUFFER_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_StorageBufferPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, storageBufferPoolDebugName.data());

            const VkDescriptorSetAllocateInfo storageBufferDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                              m_StorageBufferPool[frame], 1, &m_StorageBufferSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &storageBufferDescriptorSetAI, &m_StorageBufferSet[frame]),
                     "Failed to allocate storage buffer set!");
            const std::string storageBufferSetDebugName = std::string("VK_BINDLESS_STORAGE_BUFFER_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_StorageBufferSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, storageBufferSetDebugName.data());
        }

        {
            // FRAME DATA SET
            std::array<VkDescriptorPoolSize, 2> frameDataPoolSizes = {VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                                      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
            const VkDescriptorPoolCreateInfo frameDataPoolCI       = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,    nullptr,
                VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,  1,
                static_cast<uint32_t>(frameDataPoolSizes.size()), frameDataPoolSizes.data()};

            VK_CHECK(vkCreateDescriptorPool(logicalDevice, &frameDataPoolCI, nullptr, &m_FrameDataPool[frame]),
                     "Failed to create bindless descriptor frame data pool!");
            const std::string frameDataPoolDebugName = std::string("VK_BINDLESS_FRAME_DATA_POOL_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_FrameDataPool[frame], VK_OBJECT_TYPE_DESCRIPTOR_POOL, frameDataPoolDebugName.data());

            const VkDescriptorSetAllocateInfo frameDataDescriptorSetAI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
                                                                          m_FrameDataPool[frame], 1, &m_FrameDataSetLayout};
            VK_CHECK(vkAllocateDescriptorSets(logicalDevice, &frameDataDescriptorSetAI, &m_FrameDataSet[frame]),
                     "Failed to allocate frame data set!");
            const std::string frameDataSetDebugName = std::string("VK_BINDLESS_FRAME_DATA_SET_") + std::to_string(frame);
            VK_SetDebugName(logicalDevice, m_FrameDataSet[frame], VK_OBJECT_TYPE_DESCRIPTOR_SET, frameDataSetDebugName.data());
        }

        Renderer::GetStats().DescriptorSetCount += 3;
        Renderer::GetStats().DescriptorPoolCount += 3;
    }

    m_PCBlock.offset     = 0;
    m_PCBlock.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (Renderer::GetRendererSettings().bMeshShadingSupport)
        m_PCBlock.stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT;
    m_PCBlock.size = sizeof(PushConstantBlock);
    PFR_ASSERT(m_PCBlock.size <= 128, "Exceeding minimum limit of push constant block!");

    const std::vector<VkDescriptorSetLayout> setLayouts = {m_TextureStorageImageSetLayout, m_StorageBufferSetLayout,
                                                           m_FrameDataSetLayout};
    const VkPipelineLayoutCreateInfo pipelineLayoutCI   = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                           nullptr,
                                                           0,
                                                           static_cast<uint32_t>(setLayouts.size()),
                                                           setLayouts.data(),
                                                           1,
                                                           &m_PCBlock};
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCI, nullptr, &m_Layout), "Failed to create bindless pipeline layout!");
    VK_SetDebugName(logicalDevice, m_Layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VK_BINDLESS_PIPELINE_LAYOUT");
}

void VulkanBindlessRenderer::Destroy()
{
    Renderer::GetStats().DescriptorSetCount -= 3;
    Renderer::GetStats().DescriptorPoolCount -= 3;
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    std::ranges::for_each(m_TextureStorageImagePool,
                          [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_TextureStorageImageSetLayout, nullptr);

    std::ranges::for_each(m_StorageBufferPool,
                          [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_StorageBufferSetLayout, nullptr);

    std::ranges::for_each(m_FrameDataPool, [&](const VkDescriptorPool& pool) { vkDestroyDescriptorPool(logicalDevice, pool, nullptr); });
    vkDestroyDescriptorSetLayout(logicalDevice, m_FrameDataSetLayout, nullptr);

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);

    LOG_TAG_INFO(VULKAN, "Vulkan Bindless Renderer destroyed!");
}

}  // namespace Pathfinder