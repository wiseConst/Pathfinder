#pragma once

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

    void Bind(const Shared<CommandBuffer>& commandBuffer,
              const EPipelineStage overrideBindPoint = EPipelineStage::PIPELINE_STAGE_NONE) final override;
    void UpdateDataIfNeeded() final override;

    void LoadImage(const void* pImageInfo, uint32_t& outIndex) final override;
    void LoadTexture(const void* pTextureInfo, uint32_t& outIndex) final override;

    void LoadStorageBuffer(const void* pBufferInfo, const uint32_t binding, uint32_t& outIndex) final override;

    void FreeImage(uint32_t& imageIndex) final override;
    void FreeBuffer(uint32_t& bufferIndex, const uint32_t bufferBinding) final override;
    void FreeTexture(uint32_t& textureIndex) final override;

    NODISCARD FORCEINLINE const auto GetDescriptorSetLayouts() const
    {
        return std::vector<VkDescriptorSetLayout>{m_MegaDescriptorSetLayout};
    }
    NODISCARD FORCEINLINE const auto& GetPushConstantBlock() const { return m_PCBlock; }
    NODISCARD FORCEINLINE const auto& GetPipelineLayout() const { return m_MegaPipelineLayout; }

  private:
    std::vector<VkWriteDescriptorSet> m_Writes;

    // TODO: Apply Pool class from Core.h??
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

    VulkanDescriptorPoolPerFrame m_MegaDescriptorPool;
    VulkanDescriptorSetPerFrame m_MegaSet;

    VkPushConstantRange m_PCBlock = {};

    VkDescriptorSetLayout m_MegaDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_MegaPipelineLayout           = VK_NULL_HANDLE;

    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder