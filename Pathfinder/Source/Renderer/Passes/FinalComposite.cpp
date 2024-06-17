#include <PathfinderPCH.h>
#include "FinalComposite.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

FinalCompositePass::FinalCompositePass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void FinalCompositePass::AddPass(Unique<RenderGraph>& rendergraph)
{
    AddFinalPass(rendergraph);
    AddSwapchainBlitPass(rendergraph);
}

void FinalCompositePass::AddFinalPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGTextureID AlbedoTexture;
        RGTextureID BloomTexture;
    };

    rendergraph->AddPass<PassData>(
        "FinalCompositePass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("FinalTexture",
                                   {.DebugName  = "FinalTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT});
            builder.WriteRenderTarget("FinalTexture", glm::vec4{0.f}, EOp::CLEAR, EOp::STORE);

            pd.AlbedoTexture = builder.ReadTexture("AlbedoTexture_V1");
            pd.BloomTexture  = builder.ReadTexture("BloomTexture");
            pd.CameraData    = builder.ReadBuffer("CameraData");

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer = context.GetBuffer(pd.CameraData);
            auto& albedoTexture    = context.GetTexture(pd.AlbedoTexture);
            auto& bloomTexture     = context.GetTexture(pd.BloomTexture);

            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .StorageImageIndex  = albedoTexture->GetBindlessIndex(),
                                          .AlbedoTextureIndex = bloomTexture->GetBindlessIndex()};

            const auto& pipeline = PipelineLibrary::Get(rd->CompositePipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

void FinalCompositePass::AddSwapchainBlitPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGTextureID FinalTexture;
    };

    rendergraph->AddPass<PassData>(
        "SwapchainBlitPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder) { pd.FinalTexture = builder.ReadTexture("FinalTexture"); },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            // TODO: Insert barrier manually?
            auto& finalTexture = context.GetTexture(pd.FinalTexture);
            Application::Get().GetWindow()->CopyToWindow(finalTexture->GetImage());
        });
}

}  // namespace Pathfinder
