#include <PathfinderPCH.h>
#include "CascadedShadowMapPass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Mesh/Submesh.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

CascadedShadowMapPass::CascadedShadowMapPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void CascadedShadowMapPass::AddPass(Unique<RenderGraph>& rendergraph)
{
#if 0

    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID ShadowMapData;
        RGBufferID MeshDataOpaque;
        RGBufferID CulledMeshesOpaque;
        RGBufferID DrawBufferOpaque;
    };

    const auto& rd = Renderer::GetRendererData();
    for (uint32_t cascadeIndex{}; cascadeIndex < rd->CurrentCascadeIndex; ++cascadeIndex)
    {
        rendergraph->AddPass<PassData>(
            "CSMPass" + std::to_string(cascadeIndex), ERGPassType::RGPASS_TYPE_GRAPHICS,
            [=](PassData& pd, RenderGraphBuilder& builder)
            {
                const std::string csmTextureName = "Cascade_" + std::to_string(cascadeIndex);
                builder.DeclareTexture(csmTextureName, {.DebugName  = csmTextureName,
                                                        .Width      = m_Width,
                                                        .Height     = m_Height,
                                                        .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                        .Filter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                                        .Format     = EImageFormat::FORMAT_R8_UNORM,
                                                        .UsageFlags = EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                                      EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
                builder.WriteDepthStencil(csmTextureName, DepthStencilClearValue{1.0f, 0}, EOp::CLEAR, EOp::STORE);
                pd.CameraData = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

                builder.SetViewportScissor(m_Width, m_Height);
            },
            [=, &rd](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
            {
                //   if (IsWorldEmpty()) return;
                auto& cameraDataBuffer         = context.GetBuffer(pd.CameraData);
                auto& shadowMapDataBuffer      = context.GetBuffer(pd.ShadowMapData);
                auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaque);
                auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaque);
                auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaque);

                const PushConstantBlock pc = {.CameraDataBuffer  = cameraDataBuffer->GetBDA(),
                                              .StorageImageIndex = cascadeIndex,
                                              .addr0             = meshDataOpaqueBuffer->GetBDA(),
                                              .addr1             = culledMeshesBufferOpaque->GetBDA(),
                                              .addr2             = shadowMapDataBuffer->GetBDA()};

                const auto& pipeline = PipelineLibrary::Get(rd->CSMPipelineHash);
                Renderer::BindPipeline(cb, pipeline);
                cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
                cb->DrawMeshTasksMultiIndirect(drawBufferOpaque, sizeof(uint32_t), drawBufferOpaque, 0,
                                               (drawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) /
                                                   sizeof(DrawMeshTasksIndirectCommand),
                                               sizeof(DrawMeshTasksIndirectCommand));
            });
    }

#endif
}

}  // namespace Pathfinder
