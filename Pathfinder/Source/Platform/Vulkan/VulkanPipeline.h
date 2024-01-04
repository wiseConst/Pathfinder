#ifndef VULKANPIPELINE_H
#define VULKANPIPELINE_H

#include "Renderer/Pipeline.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanPipeline final : public Pipeline
{
  public:
    VulkanPipeline() = delete;
    explicit VulkanPipeline(const PipelineSpecification& pipelineSpec);
    ~VulkanPipeline() override { Destroy(); }

  private:
    VkPipeline m_Handle       = VK_NULL_HANDLE;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;
    PipelineSpecification m_Specification;

    void CreateLayout();

    void Invalidate() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANPIPELINE_H
