#include <PathfinderPCH.h>
#include "Pipeline.h"

#include "RendererAPI.h"
#include <Platform/Vulkan/VulkanPipeline.h>
#include "Shader.h"

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
    LOG_TRACE("{}", __FUNCTION__);
}

void PipelineBuilder::Shutdown()
{
    std::scoped_lock lock(s_PipelineBuilderMutex);
    s_PipelinesToBuild.clear();
    LOG_TRACE("{}", __FUNCTION__);
}

void PipelineBuilder::Build()
{
    std::scoped_lock lock(s_PipelineBuilderMutex);
    if (s_PipelinesToBuild.empty()) return;

    // Submit to JobSystem and wait on futures.
    std::vector<std::function<void()>> futures;
    for (const auto& pipelineSpec : s_PipelinesToBuild)
    {
        auto future = ThreadPool::Submit(
            [&]
            {
                auto pipeline = Pipeline::Create(pipelineSpec);
                PipelineLibrary::Add(pipelineSpec, pipeline);
            });

        futures.emplace_back([future] { future.get(); });
    }

#if PFR_DEBUG
    Timer t = {};
#endif

    for (auto& future : futures)
        future();

#if PFR_DEBUG
    LOG_INFO("Time taken to create ({}) pipelines: {:.2f}ms", s_PipelinesToBuild.size(), t.GetElapsedMilliseconds());
#endif
    s_PipelinesToBuild.clear();
}

void PipelineLibrary::Init()
{
    PipelineBuilder::Init();
    LOG_TRACE("{}", __FUNCTION__);
}

void PipelineLibrary::Shutdown()
{
    std::scoped_lock lock(s_PipelineLibraryMutex);
    LOG_TRACE("{}", __FUNCTION__);
    s_PipelineStorage.clear();

    PipelineBuilder::Shutdown();
}

std::size_t PipelineLibrary::PipelineSpecificationHash::operator()(const PipelineSpecification& pipelineSpec) const
{
    std::size_t hash = 0;
    switch (pipelineSpec.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            PFR_ASSERT(std::holds_alternative<GraphicsPipelineOptions>(pipelineSpec.PipelineOptions),
                       "PipelineSpecification doesn't contain GraphicsPipelineOptions!");

            const auto& gpo = std::get<GraphicsPipelineOptions>(pipelineSpec.PipelineOptions);
            for (const auto format : gpo.Formats)
            {
                hash_combine(hash, std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(format)));
            }
            hash_combine(hash, std::hash<ECullMode>{}(gpo.CullMode));
            hash_combine(hash, std::hash<EFrontFace>{}(gpo.FrontFace));
            hash_combine(hash, std::hash<EPrimitiveTopology>{}(gpo.PrimitiveTopology));
            hash_combine(hash, std::hash<EBlendMode>{}(gpo.BlendMode));
            hash_combine(hash, std::hash<EPolygonMode>{}(gpo.PolygonMode));
            hash_combine(hash, std::hash<ECompareOp>{}(gpo.DepthCompareOp));
            hash_combine(
                hash, std::hash<uint64_t>{}(gpo.bDepthTest + gpo.bDepthWrite + gpo.bDynamicPolygonMode + gpo.bMeshShading + gpo.LineWidth));

            for (size_t i{}; i < gpo.VertexStreams.size(); ++i)
            {
                const auto& bufferElements = gpo.VertexStreams[i].GetElements();

                for (size_t k{}; k < bufferElements.size(); ++k)
                {
                    hash_combine(hash, std::hash<std::string>{}(bufferElements[k].Name));
                    hash_combine(hash, std::hash<uint64_t>{}(bufferElements[k].Offset));
                    hash_combine(hash, std::hash<EShaderBufferElementType>{}(bufferElements[k].Type));
                }
            }

            break;
        }
        case EPipelineType::PIPELINE_TYPE_COMPUTE:
        {
            PFR_ASSERT(std::holds_alternative<ComputePipelineOptions>(pipelineSpec.PipelineOptions),
                       "PipelineSpecification doesn't contain ComputePipelineOptions!");

            const auto& cpo = std::get<ComputePipelineOptions>(pipelineSpec.PipelineOptions);

            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {

            PFR_ASSERT(std::holds_alternative<RayTracingPipelineOptions>(pipelineSpec.PipelineOptions),
                       "PipelineSpecification doesn't contain RayTracingPipelineOptions!");

            const auto& rtpo = std::get<RayTracingPipelineOptions>(pipelineSpec.PipelineOptions);
            hash_combine(hash, std::hash<size_t>{}(rtpo.MaxPipelineRayRecursionDepth));

            break;
        }
    }

    for (const auto& [stage, constants] : pipelineSpec.ShaderConstantsMap)
    {
        hash_combine(hash, std::hash<EShaderStage>{}(stage));

        for (const auto& constant : constants)
        {
            hash_combine(hash, std::hash<std::decay_t<decltype(constant)>>{}(constant));
        }
    }

    for (const auto& [definition, value] : pipelineSpec.Shader->GetSpecification().MacroDefinitions)
    {
        hash_combine(hash, std::hash<std::string>{}(definition));
        hash_combine(hash, std::hash<std::string>{}(value));
    }
    hash_combine(hash, std::hash<std::string>{}(pipelineSpec.Shader->GetSpecification().Name));
    hash_combine(hash, std::hash<uint64_t>{}(static_cast<uint64_t>(pipelineSpec.PipelineType)));

    LOG_WARN("Pipeline <{}> got hash: ({}).", pipelineSpec.DebugName, hash);
    return hash;
}

}  // namespace Pathfinder