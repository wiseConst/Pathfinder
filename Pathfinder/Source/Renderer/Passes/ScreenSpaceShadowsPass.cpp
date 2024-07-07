#include <PathfinderPCH.h>
#include "ScreenSpaceShadowsPass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

ScreenSpaceShadowsPass::ScreenSpaceShadowsPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void ScreenSpaceShadowsPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID LightData;
        RGBufferID CulledPointLightIndices;
        RGBufferID CulledSpotLightIndices;
        RGTextureID DepthOpaque;
        RGTextureID SSSTexture;
    };

    rendergraph->AddPass<PassData>(
        "ScreenSpaceShadowsPass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.DepthOpaque = builder.ReadTexture("DepthOpaque", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.CameraData  = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.CulledPointLightIndices =
                builder.ReadBuffer("CulledPointLightIndices", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.CulledSpotLightIndices =
                builder.ReadBuffer("CulledSpotLightIndices", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.LightData = builder.ReadBuffer("LightData", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);

            builder.DeclareTexture("SSSTexture",
                                   {.DebugName  = "SSSTexture",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                    .Format     = EImageFormat::FORMAT_R16_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT});
            pd.SSSTexture = builder.WriteTexture("SSSTexture");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //   if (IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            // TODO: remove it
            if (!rd->bAnybodyCastsShadows) return;

            auto& depthOpaqueTexture            = context.GetTexture(pd.DepthOpaque);
            auto& sssTexture                    = context.GetTexture(pd.SSSTexture);
            auto& cameraDataBuffer              = context.GetBuffer(pd.CameraData);
            auto& lightDataBuffer               = context.GetBuffer(pd.LightData);
            auto& culledPointLightIndicesBuffer = context.GetBuffer(pd.CulledPointLightIndices);
            auto& culledSpotLightIndicesBuffer  = context.GetBuffer(pd.CulledSpotLightIndices);

            // TODO: CLEAN SSS TEXTURE BEFORE DISPATCH
            // sssTexture->GetImage()->ClearColor(cb, glm::vec4{1.f});

            const PushConstantBlock pc = {.CameraDataBuffer                   = cameraDataBuffer->GetBDA(),
                                          .LightDataBuffer                    = lightDataBuffer->GetBDA(),
                                          .StorageImageIndex                  = sssTexture->GetStorageTextureBindlessIndex(),
                                          .AlbedoTextureIndex                 = depthOpaqueTexture->GetTextureBindlessIndex(),
                                          .VisiblePointLightIndicesDataBuffer = culledPointLightIndicesBuffer->GetBDA(),
                                          .VisibleSpotLightIndicesDataBuffer  = culledSpotLightIndicesBuffer->GetBDA()};

            const auto& pipeline = PipelineLibrary::Get(rd->SSShadowsPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Dispatch(glm::ceil((float)m_Width / SSS_LOCAL_GROUP_SIZE), glm::ceil((float)m_Height / SSS_LOCAL_GROUP_SIZE));
        });
}

}  // namespace Pathfinder
