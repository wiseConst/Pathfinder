#include <PathfinderPCH.h>
#include "AOBlur.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

AOBlurPass::AOBlurPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void AOBlurPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGTextureID SSAOTexture;
    };

    rendergraph->AddPass<PassData>(
        "AOBlur Pass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.CameraData  = builder.ReadBuffer("CameraData");
            pd.SSAOTexture = builder.ReadTexture("SSAOTexture");

            builder.DeclareTexture("AOBlurTexture",
                                   {.DebugName  = "AOBlurTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_R8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("AOBlurTexture", glm::vec4{1.f}, EOp::CLEAR, EOp::STORE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            // if (IsWorldEmpty()) return;
            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer = context.GetBuffer(pd.CameraData);
            auto& ssaoTexture      = context.GetTexture(pd.SSAOTexture);

            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .AlbedoTextureIndex = ssaoTexture->GetBindlessIndex()};
            const auto& pipeline       = PipelineLibrary::Get(rd->BoxBlurAOPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

}  // namespace Pathfinder
