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
        "AOBlurPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.CameraData  = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.SSAOTexture = builder.ReadTexture("SSAOTexture", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.DeclareTexture("AOBlurTexture",
                                   {.DebugName  = "AOBlurTexture",
                                    .Dimensions = glm::uvec3{m_Width, m_Height, 1},
                                    .WrapS       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .MagFilter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
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
                                          .AlbedoTextureIndex = ssaoTexture->GetTextureBindlessIndex()};
            const auto& pipeline       = PipelineLibrary::Get(rd->BoxBlurAOPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

}  // namespace Pathfinder
