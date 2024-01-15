#ifndef VULKANBINDLESSRENDERER_H
#define VULKANBINDLESSRENDERER_H

#include "Renderer/BindlessRenderer.h"
#include "VulkanCore.h"
#include "VulkanDescriptors.h"

namespace Pathfinder
{

class VulkanBindlessRenderer final : public BindlessRenderer
{
  public:
    VulkanBindlessRenderer();
    ~VulkanBindlessRenderer() override { Destroy(); }

    void Bind(const Shared<CommandBuffer>& commandBuffer, const bool bGraphicsBindPoint = true) final override;

    void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) final override;

    void LoadImage(const ImagePerFrame& images) final override;
    void LoadImage(const Shared<Image>& image) final override;

    void FreeImage(uint32_t& imageIndex);

    NODISCARD FORCEINLINE const auto& GetTextureSetLayout() const { return m_TextureSetLayout; }
    NODISCARD FORCEINLINE const auto& GetImageSetLayout() const { return m_ImageSetLayout; }
    NODISCARD FORCEINLINE const VkPushConstantRange& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_Layout; }

  private:
    VkDescriptorSetLayout m_TextureSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_TexturePool;
    VulkanDescriptorSetPerFrame m_TextureSet;

    VkDescriptorSetLayout m_ImageSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_ImagePool;
    VulkanDescriptorSetPerFrame m_ImageSet;

    VkDescriptorSetLayout m_CameraSetLayout = VK_NULL_HANDLE;
    VulkanDescriptorPoolPerFrame m_CameraPool;
    VulkanDescriptorSetPerFrame m_CameraSet;

    VkPushConstantRange m_PCBlock;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;

    // TODO: Maybe it's better to hold an array of weak ptrs?
    std::vector<uint32_t> m_ImageIndicesPool;
    std::vector<uint32_t> m_FreeImageIndicesPool;

    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBINDLESSRENDERER_H
