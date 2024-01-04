#include "PathfinderPCH.h"
#include "Pipeline.h"

#include "Renderer/RendererAPI.h"
#include "Platform/Vulkan/VulkanPipeline.h"

namespace Pathfinder
{

Shared<Pipeline> Pipeline::Create(const PipelineSpecification& pipelineSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanPipeline>(pipelineSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

void PipelineBuilder::Init()
{
    LOG_TAG_INFO(RENDERER, "Pipeline builder created! (CURRENTLY NOT IMPLEMENTED AT ALL)");
}

void PipelineBuilder::Shutdown()
{
    LOG_TAG_INFO(RENDERER, "Pipeline builder destroyed! (CURRENTLY NOT IMPLEMENTED AT ALL)");
}

void PipelineBuilder::Push(Unique<Pipeline>& pipeline, const PipelineSpecification pipelineSpec)
{
    //  auto pair = std::make_pair<pipelineSpec.DebugName, std::make_pair<pipeline, pipelineSpec>>;
    // s_PipelinesToBuild.push_back(pair);
}

void PipelineBuilder::Build()
{
    if (s_PipelinesToBuild.empty()) return;

    //  The way it should work:
    // Submit to jobsystem and wait on futures

    std::vector<std::thread> workers;
}

}  // namespace Pathfinder