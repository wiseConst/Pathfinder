#include <PathfinderPCH.h>
#include "DepthPrePass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Mesh/Submesh.h>
#include <Renderer/Material.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

DepthPrePass::DepthPrePass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void DepthPrePass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID MeshDataOpaque;
        RGBufferID CameraData;
        RGBufferID DrawBufferOpaque;
        RGBufferID CulledMeshesOpaque;
    };

    rendergraph->AddPass<PassData>(
        "DepthPre Pass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("DepthOpaque", {.DebugName  = "DepthOpaque",
                                                   .Width      = m_Width,
                                                   .Height     = m_Height,
                                                   .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                   .Filter     = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                                   .Format     = EImageFormat::FORMAT_D32F,
                                                   .UsageFlags = EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                                 EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteDepthStencil("DepthOpaque", DepthStencilClearValue(0.f, 0), EOp::CLEAR, EOp::STORE);

            pd.CameraData         = builder.ReadBuffer("CameraData");
            pd.MeshDataOpaque     = builder.ReadBuffer("MeshDataOpaque_V0");
            pd.DrawBufferOpaque   = builder.ReadBuffer("DrawBufferOpaque_V0");
            pd.CulledMeshesOpaque = builder.ReadBuffer("CulledMeshesOpaque_V0");

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer         = context.GetBuffer(pd.CameraData);
            auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaque);
            auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaque);
            auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaque);

            const PushConstantBlock pc = {.CameraDataBuffer = cameraDataBuffer->GetBDA(),
                                          .addr0            = meshDataOpaqueBuffer->GetBDA(),
                                          .addr1            = culledMeshesBufferOpaque->GetBDA()};

            const auto& pipeline = PipelineLibrary::Get(rd->DepthPrePassPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->DrawMeshTasksMultiIndirect(drawBufferOpaque, sizeof(uint32_t), drawBufferOpaque, 0,
                                           (drawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) /
                                               sizeof(DrawMeshTasksIndirectCommand),
                                           sizeof(DrawMeshTasksIndirectCommand));
        });
}

}  // namespace Pathfinder
