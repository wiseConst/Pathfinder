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

    void Bind(const Shared<CommandBuffer>& commandBuffer) final override;
    void UpdateDataIfNeeded() final override;

    void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) final override;

    void LoadImage(const ImagePerFrame& images) final override;
    void LoadImage(const Shared<Image>& image) final override;
    void LoadTexture(const Shared<Texture2D>& texture) final override;

    void LoadVertexPosBuffer(const Shared<Buffer>& buffer) final override;
    void LoadVertexAttribBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletVerticesBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletTrianglesBuffer(const Shared<Buffer>& buffer) final override;

    void FreeImage(uint32_t& imageIndex) final override;
    void FreeBuffer(uint32_t& bufferIndex) final override;
    void FreeTexture(uint32_t& textureIndex) final override;

    NODISCARD FORCEINLINE const auto& GetTextureSetLayout() const { return m_TextureSetLayout; }
    NODISCARD FORCEINLINE const auto& GetImageSetLayout() const { return m_ImageSetLayout; }
    NODISCARD FORCEINLINE const auto& GetStorageBufferSetLayout() const { return m_StorageBufferSetLayout; }
    NODISCARD FORCEINLINE const auto& GetCameraSetLayout() const { return m_CameraSetLayout; }
    NODISCARD FORCEINLINE const VkPushConstantRange& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_Layout; }

  private:
    std::vector<VkWriteDescriptorSet> m_Writes;

    // TODO: Maybe it's better to hold an array of weak ptrs of textures?
    std::vector<uint32_t> m_TextureIndicesPool;
    std::vector<uint32_t> m_FreeTextureIndicesPool;

    // TODO: Maybe it's better to hold an array of weak ptrs of images?
    std::vector<uint32_t> m_ImageIndicesPool;
    std::vector<uint32_t> m_FreeImageIndicesPool;

    // TODO: Make use of it somehow
    //  using StorageBufferPoolPerBinding = std::unordered_map<uint32_t, std::vector<uint32_t>>;
    std::vector<uint32_t> m_StorageBufferIndicesPool;
    std::vector<uint32_t> m_FreeStorageBufferIndicesPool;

    VulkanDescriptorPoolPerFrame m_TexturePool;
    VulkanDescriptorSetPerFrame m_TextureSet;

    VulkanDescriptorPoolPerFrame m_ImagePool;
    VulkanDescriptorSetPerFrame m_ImageSet;

    VulkanDescriptorPoolPerFrame m_StorageBufferPool;
    VulkanDescriptorSetPerFrame m_StorageBufferSet;

    VulkanDescriptorPoolPerFrame m_CameraPool;
    VulkanDescriptorSetPerFrame m_CameraSet;

    VkPushConstantRange m_PCBlock = {};

    VkDescriptorSetLayout m_TextureSetLayout       = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ImageSetLayout         = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_StorageBufferSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_CameraSetLayout        = VK_NULL_HANDLE;

    VkPipelineLayout m_Layout = VK_NULL_HANDLE;

    void LoadStorageBufferInternal(const Shared<Buffer>& buffer, const uint32_t binding);
    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBINDLESSRENDERER_H
