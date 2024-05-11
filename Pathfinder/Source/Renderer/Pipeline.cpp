#include "PathfinderPCH.h"
#include "Pipeline.h"

#include "Core/CoreUtils.h"
#include "Renderer/RendererAPI.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Framebuffer.h"

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
    std::scoped_lock lock(m_PipelineBuilderMutex);
    LOG_TAG_INFO(RENDERER, "Pipeline builder destroyed!");
    s_PipelinesToBuild.clear();
}

void PipelineBuilder::Build()
{
    std::scoped_lock lock(m_PipelineBuilderMutex);
    if (s_PipelinesToBuild.empty()) return;

    // Submit to JobSystem and wait on futures.
    std::vector<std::function<void()>> futures;
    for (auto& [pipeline, pipelineSpec] : s_PipelinesToBuild)
    {
        auto future = JobSystem::Submit(
            [&]
            {
                pipeline = Pipeline::Create(pipelineSpec);
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
    LOG_TAG_INFO(RENDERER, "Time taken to create (%zu) pipelines: %0.2fms", s_PipelinesToBuild.size(), t.GetElapsedMilliseconds());
#endif
    s_PipelinesToBuild.clear();
}

void PipelineLibrary::Init()
{
    PipelineBuilder::Init();
    LOG_TAG_INFO(RENDERER, "Pipeline library created!");
}

void PipelineLibrary::Shutdown()
{
    std::scoped_lock lock(m_PipelineLibraryMutex);
    LOG_TAG_INFO(RENDERER, "Pipeline library destroyed!");
    s_PipelineStorage.clear();

    PipelineBuilder::Shutdown();
}

std::size_t PipelineLibrary::PipelineSpecificationHash::operator()(const PipelineSpecification& pipelineSpec) const
{
    std::size_t hash = std::hash<std::string>{}(pipelineSpec.DebugName);

    // TODO: Look at std::visitor
    switch (pipelineSpec.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            const auto* GPO = std::get_if<GraphicsPipelineOptions>(&pipelineSpec.PipelineOptions.value());
            if (!GPO)
            {
                LOG_WARN("GraphicsPipelineOptions is not valid!");
                break;
            }

            hash_combine(hash, std::hash<std::string>{}(GPO->TargetFramebuffer->GetSpecification().Name));
            hash_combine(hash, std::hash<ECullMode>{}(GPO->CullMode));
            hash_combine(hash, std::hash<EFrontFace>{}(GPO->FrontFace));
            hash_combine(hash, std::hash<EPrimitiveTopology>{}(GPO->PrimitiveTopology));
            hash_combine(hash, std::hash<EBlendMode>{}(GPO->BlendMode));
            hash_combine(hash, std::hash<EPolygonMode>{}(GPO->PolygonMode));
            hash_combine(hash, std::hash<ECompareOp>{}(GPO->DepthCompareOp));
            hash_combine(hash, std::hash<uint64_t>{}(GPO->bDepthTest + GPO->bDepthWrite + GPO->bDynamicPolygonMode + GPO->bMeshShading +
                                                     GPO->LineWidth));

            for (size_t i{}; i < GPO->InputBufferBindings.size(); ++i)
            {
                const auto& bufferElements = GPO->InputBufferBindings[i].GetElements();

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
            const auto* CPO = std::get_if<ComputePipelineOptions>(&pipelineSpec.PipelineOptions.value());
            if (!CPO)
            {
                LOG_WARN("GraphicsPipelineOptions is not valid!");
                break;
            }

            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {
            const auto* RTPO = std::get_if<RayTracingPipelineOptions>(&pipelineSpec.PipelineOptions.value());
            if (!RTPO)
            {
                LOG_WARN("GraphicsPipelineOptions is not valid!");
                break;
            }

            hash_combine(hash, std::hash<size_t>{}(RTPO->MaxPipelineRayRecursionDepth));

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
    hash_combine(hash, std::hash<uint64_t>{}(static_cast<uint64_t>(pipelineSpec.bBindlessCompatible)));

    LOG_TAG_WARN(PIPELINE_LIBRARY, "Pipeline <%s%> got hash: (%zu).", pipelineSpec.DebugName.data(), hash);
    return hash;
}

}  // namespace Pathfinder