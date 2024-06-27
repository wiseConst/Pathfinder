#include <PathfinderPCH.h>
#include "BloomPass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

BloomPass::BloomPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void BloomPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    AddHorizontalPass(rendergraph);
    AddVerticalPass(rendergraph);
}

void BloomPass::AddHorizontalPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGTextureID HDRTexture;
    };

    rendergraph->AddPass<PassData>(
        "BloomHorizontalPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("BloomTextureHoriz",
                                   {.DebugName  = "BloomTextureHoriz",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_RGBA16F,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("BloomTextureHoriz", glm::vec4{0.f}, EOp::CLEAR, EOp::STORE);

            pd.HDRTexture = builder.ReadTexture("HDRTexture_V2", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.CameraData = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            const auto& rd       = Renderer::GetRendererData();
            const auto& pipeline = PipelineLibrary::Get(rd->BloomPipelineHash.at(0));

            auto& cameraDataBuffer = context.GetBuffer(pd.CameraData);
            auto& hdrTexture       = context.GetTexture(pd.HDRTexture);

            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .AlbedoTextureIndex = hdrTexture->GetBindlessIndex()};

            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

void BloomPass::AddVerticalPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGTextureID BloomTextureHoriz;
    };

    rendergraph->AddPass<PassData>(
        "BloomVerticalPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.DeclareTexture("BloomTexture",
                                   {.DebugName  = "BloomTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_RGBA16F,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("BloomTexture", glm::vec4{0.f}, EOp::CLEAR, EOp::STORE);

            pd.BloomTextureHoriz = builder.ReadTexture("BloomTextureHoriz", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.CameraData        = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            const auto& rd       = Renderer::GetRendererData();
            const auto& pipeline = PipelineLibrary::Get(rd->BloomPipelineHash.at(1));

            auto& cameraDataBuffer     = context.GetBuffer(pd.CameraData);
            auto& bloomTextureHoriz    = context.GetTexture(pd.BloomTextureHoriz);
            const PushConstantBlock pc = {.CameraDataBuffer   = cameraDataBuffer->GetBDA(),
                                          .AlbedoTextureIndex = bloomTextureHoriz->GetBindlessIndex()};

            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(3);
        });
}

}  // namespace Pathfinder
