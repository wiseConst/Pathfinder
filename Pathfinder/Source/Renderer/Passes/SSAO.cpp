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
        "SSAOPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("SSAOTexture",
                                   {.DebugName  = "SSAOTexture",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_R8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("SSAOTexture", ColorClearValue{1.f}, EOp::CLEAR, EOp::STORE);
            pd.CameraData  = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.DepthOpaque = builder.ReadTexture("DepthOpaque", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //   if (IsWorldEmpty()) return;
            const auto& rd           = Renderer::GetRendererData();
            auto& cameraDataBuffer   = context.GetBuffer(pd.CameraData);
            auto& depthOpaqueTexture = context.GetTexture(pd.DepthOpaque);

            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .StorageImageIndex  = rd->AONoiseTexture->GetTextureBindlessIndex(),
                                          .AlbedoTextureIndex = depthOpaqueTexture->GetTextureBindlessIndex()};

            const auto& pipeline = PipelineLibrary::Get(rd->SSAOPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

}  // namespace Pathfinder
