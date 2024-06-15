#include <PathfinderPCH.h>
#include "SSAO.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

SSAOPass::SSAOPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void SSAOPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGTextureID DepthOpaque;
    };

    rendergraph->AddPass<PassData>(
        "SSAO Pass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("SSAOTexture",
                                   {.DebugName  = "SSAOTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_R8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("SSAOTexture", ColorClearValue{1.f}, EOp::CLEAR, EOp::STORE);
            pd.CameraData  = builder.ReadBuffer("CameraData");
            pd.DepthOpaque = builder.ReadTexture("DepthOpaque");

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //   if (IsWorldEmpty()) return;
            const auto& rd           = Renderer::GetRendererData();
            auto& cameraDataBuffer   = context.GetBuffer(pd.CameraData);
            auto& depthOpaqueTexture = context.GetTexture(pd.DepthOpaque);

            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .StorageImageIndex  = rd->AONoiseTexture->GetBindlessIndex(),
                                          .AlbedoTextureIndex = depthOpaqueTexture->GetBindlessIndex()};

            const auto& pipeline = PipelineLibrary::Get(rd->SSAOPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

}  // namespace Pathfinder
