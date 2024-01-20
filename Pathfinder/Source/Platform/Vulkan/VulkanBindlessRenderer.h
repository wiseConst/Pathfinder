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

    void Bind(const Shared<CommandBuffer>& commandBuffer, const bool bGraphicsBindPoint = true) final override;

    void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) final override;

    void LoadImage(const ImagePerFrame& images) final override;
    void LoadImage(const Shared<Image>& image) final override;

    void LoadVertexPosBuffer(const Shared<Buffer>& buffer) final override;
    void LoadVertexAttribBuffer(const Shared<Buffer>& buffer) final override;
    void LoadMeshletBuffer(const Shared<Buffer>& buffer) final override;

    void FreeImage(uint32_t& imageIndex) final override;
    void FreeBuffer(uint32_t& bufferIndex) final override;

    NODISCARD FORCEINLINE const auto& GetTextureSetLayout() const { return m_TextureSetLayout; }
    NODISCARD FORCEINLINE const auto& GetImageSetLayout() const { return m_ImageSetLayout; }
    NODISCARD FORCEINLINE const auto& GetStorageBufferSetLayout() const { return m_StorageBufferSetLayout; }
    NODISCARD FORCEINLINE const auto& GetCameraSetLayout() const { return m_CameraSetLayout; }
    NODISCARD FORCEINLINE const VkPushConstantRange& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_Layout; }

  private:
    VkDescriptorSetLayout m_TextureSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_TexturePool;
    VulkanDescriptorSetPerFrame m_TextureSet;

    VkDescriptorSetLayout m_ImageSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_ImagePool;
    VulkanDescriptorSetPerFrame m_ImageSet;

    VkDescriptorSetLayout m_StorageBufferSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_StorageBufferPool;
    VulkanDescriptorSetPerFrame m_StorageBufferSet;

    VkDescriptorSetLayout m_CameraSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_CameraPool;
    VulkanDescriptorSetPerFrame m_CameraSet;

    VkPushConstantRange m_PCBlock = {};
    VkPipelineLayout m_Layout     = VK_NULL_HANDLE;

    // TODO: Maybe it's better to hold an array of weak ptrs of images?
    std::vector<uint32_t> m_ImageIndicesPool;
    std::vector<uint32_t> m_FreeImageIndicesPool;

    std::vector<uint32_t> m_StorageBufferIndicesPool;
    std::vector<uint32_t> m_FreeStorageBufferIndicesPool;

    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBINDLESSRENDERER_H
