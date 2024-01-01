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

}  // namespace Pathfinder