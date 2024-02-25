#ifndef VULKANBINDLESSRENDERER_H
#define VULKANBINDLESSRENDERER_H

#include "Renderer/BindlessRenderer.h"
#include "VulkanCore.h"
#include "VulkanDescriptors.h"

namespace Pathfinder
{
// CORE: https://registry.khronos.org/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility

class VulkanBindlessRenderer final : public BindlessRenderer
{
  public:
    VulkanBindlessRenderer();
    ~VulkanBindlessRenderer() override { Destroy(); }

    void Bind(const Shared<CommandBuffer>& commandBuffer,
              const EPipelineStage overrideBindPoint = EPipelineStage::PIPELINE_STAGE_NONE) final override;
    void UpdateDataIfNeeded() final override;

    void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) final override;
    void UpdateLightData(const Shared<Buffer>& lightDataUniformBuffer) final override;

    void LoadImage(const ImagePerFrame& images) final override;
    void LoadImage(const Shared<Image>& image) final override;
    void LoadTexture(const Shared<Texture2D>& texture) final override;

    void LoadVertexPosBuffer(const Shared<Buffer>& buffer) final override;
    void LoadVertexAttribBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletVerticesBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletTrianglesBuffer(const Shared<Buffer>& buffer) final override;

    void FreeImage(uint32_t& imageIndex) final override;
    void FreeBuffer(uint32_t& bufferIndex, uint32_t bufferBinding) final override;
    void FreeTexture(uint32_t& textureIndex) final override;

    NODISCARD FORCEINLINE const auto& GetTextureStorageImageSetLayout() const { return m_TextureStorageImageSetLayout; }
    NODISCARD FORCEINLINE const auto& GetStorageBufferSetLayout() const { return m_StorageBufferSetLayout; }
    NODISCARD FORCEINLINE const auto& GetFrameDataSetLayout() const { return m_FrameDataSetLayout; }
    NODISCARD FORCEINLINE const VkPushConstantRange& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_Layout; }

  private:
    std::vector<VkWriteDescriptorSet> m_Writes;

    struct IndicesPool
    {
        std::vector<uint32_t> Busy;
        std::vector<uint32_t> Free;
    };

    // TODO: Maybe it's better to hold an array of weak ptrs of textures/images?
    IndicesPool m_TexturePool;
    IndicesPool m_StorageImagePool;

    // NOTE: key - binding, value - pool
    std::unordered_map<uint32_t, IndicesPool> m_StorageBufferIndicesPool;

    VulkanDescriptorPoolPerFrame m_TextureStorageImagePool;
    VulkanDescriptorSetPerFrame m_TextureStorageImageSet;

    VulkanDescriptorPoolPerFrame m_StorageBufferPool;
    VulkanDescriptorSetPerFrame m_StorageBufferSet;

    VulkanDescriptorPoolPerFrame m_FrameDataPool;
    VulkanDescriptorSetPerFrame m_FrameDataSet;

    VkPushConstantRange m_PCBlock = {};

    VkDescriptorSetLayout m_TextureStorageImageSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_StorageBufferSetLayout       = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_FrameDataSetLayout           = VK_NULL_HANDLE;

    VkPipelineLayout m_Layout = VK_NULL_HANDLE;

    void LoadStorageBufferInternal(const Shared<Buffer>& buffer, const uint32_t binding);
    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBINDLESSRENDERER_H
