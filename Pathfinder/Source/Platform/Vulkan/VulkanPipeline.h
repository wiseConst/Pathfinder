#pragma once

#include "Renderer/Pipeline.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanPipeline final : public Pipeline
{
  public:
    explicit VulkanPipeline(const PipelineSpecification& pipelineSpec);
    ~VulkanPipeline() override { Destroy(); }

    NODISCARD FORCEINLINE VkPipelineLayout GetLayout() const { return m_Layout; }
    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }

    NODISCARD FORCEINLINE VkShaderStageFlags GetPushConstantShaderStageByIndex(const uint32_t pushConstantIndex) const
    {
        return m_PushConstants[pushConstantIndex].stageFlags;
    }

  private:
    std::vector<VkPushConstantRange> m_PushConstants;  // FIXME: Wasting memory, but convenient way to bind push constants
    VkPipeline m_Handle       = VK_NULL_HANDLE;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;

    VulkanPipeline() = delete;
    void CreateLayout();
    void Invalidate() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder
