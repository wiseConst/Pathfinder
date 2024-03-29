#include "PathfinderPCH.h"
#include "Pipeline.h"

#include "Core/CoreUtils.h"
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
    LOG_TAG_INFO(RENDERER, "Pipeline builder created!");
}

void PipelineBuilder::Shutdown()
{
    LOG_TAG_INFO(RENDERER, "Pipeline builder destroyed!");
    s_PipelinesToBuild.clear();
}

// FIXME: JobSystem gonna stuck(won't propogate work) if pipeline count > threads
void PipelineBuilder::Build()
{
    if (s_PipelinesToBuild.empty()) return;

    // The way it should work: Submit to jobsystem and wait on futures
    std::vector<std::function<void()>> futures;
    for (auto& [pipeline, pipelineSpec] : s_PipelinesToBuild)
    {
        auto future = JobSystem::Submit([&] { pipeline = Pipeline::Create(pipelineSpec); });

        futures.emplace_back([future] { future.get(); });
    }

    Timer t = {};
    for (auto& future : futures)
        future();

    LOG_TAG_INFO(RENDERER, "Time taken to create (%zu) pipelines: %0.2fms", s_PipelinesToBuild.size(), t.GetElapsedSeconds() * 1000);
    s_PipelinesToBuild.clear();
}

}  // namespace Pathfinder