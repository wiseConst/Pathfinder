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

    NODISCARD FORCEINLINE VkPipelineLayout GetLayout() const { return m_Layout; }
    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE const PipelineSpecification& GetSpecification() const final override { return m_Specification; }

    FORCEINLINE void SetPolygonMode(const EPolygonMode polygonMode) final override { m_Specification.PolygonMode = polygonMode; }

  private:
    VkPipeline m_Handle       = VK_NULL_HANDLE;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;
    PipelineSpecification m_Specification={};

    void CreateLayout();

    void Invalidate() final override;
    void Destroy() final override;

    void CreateOrRetrieveAndValidatePipelineCache(VkPipelineCache& outCache, const std::string& pipelineName) const;
    void SavePipelineCache(VkPipelineCache& cache, const std::string& pipelineName) const;
};

}  // namespace Pathfinder

#endif  // VULKANPIPELINE_H
