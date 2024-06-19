#pragma once

#include "VulkanCore.h"
#include <vector>

#include <Renderer/DescriptorManager.h>

namespace Pathfinder
{
class VulkanDescriptorManager final : public DescriptorManager
{
  public:
    VulkanDescriptorManager();
    ~VulkanDescriptorManager() override { Destroy(); }

    void Bind(const Shared<CommandBuffer>& commandBuffer,
              const EPipelineStage overrideBindPoint = EPipelineStage::PIPELINE_STAGE_NONE) final override;

    void LoadImage(const void* pImageInfo, Optional<uint32_t>& outIndex) final override;
    void LoadTexture(const void* pTextureInfo, Optional<uint32_t>& outIndex) final override;

    FORCEINLINE void FreeImage(Optional<uint32_t>& imageIndex) final override
    {
        m_StorageImageIDPool.Release(imageIndex.value());
        imageIndex = std::nullopt;
    }

    FORCEINLINE void FreeTexture(Optional<uint32_t>& textureIndex) final override
    {
        m_TextureIDPool.Release(textureIndex.value());
        textureIndex = std::nullopt;
    }

    NODISCARD FORCEINLINE const auto GetDescriptorSetLayouts() const
    {
        return std::vector<VkDescriptorSetLayout>{m_MegaDescriptorSetLayout};
    }
    NODISCARD FORCEINLINE const auto& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_MegaPipelineLayout; }

  private:
    using VulkanDescriptorPoolPerFrame = std::array<VkDescriptorPool, s_FRAMES_IN_FLIGHT>;
    using VulkanDescriptorSetPerFrame  = std::array<VkDescriptorSet, s_FRAMES_IN_FLIGHT>;

    VulkanDescriptorPoolPerFrame m_MegaDescriptorPool;
    VulkanDescriptorSetPerFrame m_MegaSet;

    VkPushConstantRange m_PCBlock = {};

    VkDescriptorSetLayout m_MegaDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_MegaPipelineLayout           = VK_NULL_HANDLE;

    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder
