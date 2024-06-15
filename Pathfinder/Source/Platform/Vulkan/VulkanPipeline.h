#pragma once

#include <Renderer/Pipeline.h>
#include "VulkanCore.h"

namespace Pathfinder
{

// NOTE: No push constants stored, since I use the bindless's.
class VulkanPipeline final : public Pipeline
{
  public:
    explicit VulkanPipeline(const PipelineSpecification& pipelineSpec);
    ~VulkanPipeline() override { Destroy(); }

    NODISCARD FORCEINLINE VkPipelineLayout GetLayout() const { return m_Layout; }
    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }

  private:
    VkPipeline m_Handle       = VK_NULL_HANDLE;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;

    VulkanPipeline() = delete;
    void Invalidate() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder
