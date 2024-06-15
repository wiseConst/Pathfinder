#include <PathfinderPCH.h>
#include "LightCulling.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

LightCullingPass::LightCullingPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void LightCullingPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    if (m_bRecomputeLightCullingFrustums) AddComputeLightCullingFrustumsPass(rendergraph);
    AddLightCullingPass(rendergraph);
}

void LightCullingPass::AddLightCullingPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID LightData;
        RGBufferID LightCullFrustums;
        RGTextureID DepthOpaque;
        RGTextureID FrustumDebugTexture;
        RGTextureID LightHeatMapTexture;
        RGBufferID CulledPointLightIndices;
        RGBufferID CulledSpotLightIndices;
    };

    rendergraph->AddPass<PassData>(
        "LightCulling Pass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.CameraData        = builder.ReadBuffer("CameraData");
            pd.LightData         = builder.ReadBuffer("LightData");
            pd.LightCullFrustums = builder.ReadBuffer("LightCullFrustums");
            pd.DepthOpaque       = builder.ReadTexture("DepthOpaque");

            // TODO: Move  FrustumDebugTexture to ComputeFrustums pass
            builder.DeclareTexture("FrustumDebugTexture",
                                   {.DebugName  = "FrustumDebugTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT});
            pd.FrustumDebugTexture = builder.WriteTexture("FrustumDebugTexture");

            builder.DeclareTexture("LightHeatMapTexture",
                                   {.DebugName  = "LightHeatMapTexture",
                                    .Width      = m_Width,
                                    .Height     = m_Height,
                                    .Wrap       = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .Filter     = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT});
            pd.LightHeatMapTexture = builder.WriteTexture("LightHeatMapTexture");

            const uint32_t adjustedTiledWidth  = glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE);
            const uint32_t adjustedTiledHeight = glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE);

            const size_t cplibSize = MAX_POINT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;
            builder.DeclareBuffer("CulledPointLightIndices",
                                  {.DebugName  = "CulledPointLightIndices",
                                   .UsageFlags = EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS | EBufferUsage::BUFFER_USAGE_STORAGE,
                                   .Capacity   = cplibSize});
            pd.CulledPointLightIndices = builder.WriteBuffer("CulledPointLightIndices");

            const size_t csplibSize = MAX_SPOT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;
            builder.DeclareBuffer("CulledSpotLightIndices",
                                  {.DebugName  = "CulledSpotLightIndices",
                                   .UsageFlags = EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS | EBufferUsage::BUFFER_USAGE_STORAGE,
                                   .Capacity   = csplibSize});
            pd.CulledSpotLightIndices = builder.WriteBuffer("CulledSpotLightIndices");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //    if (IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();
            if (rd->LightStruct->PointLightCount == 0 && rd->LightStruct->SpotLightCount == 0) return;

            auto& depthOpaqueTexture            = context.GetTexture(pd.DepthOpaque);
            auto& frustumDebugTexture           = context.GetTexture(pd.FrustumDebugTexture);
            auto& lightHeatMapTexture           = context.GetTexture(pd.LightHeatMapTexture);
            auto& cameraDataBuffer              = context.GetBuffer(pd.CameraData);
            auto& lightDataBuffer               = context.GetBuffer(pd.LightData);
            auto& lightCullFrustumsDataBuffer   = context.GetBuffer(pd.LightCullFrustums);
            auto& culledPointLightIndicesBuffer = context.GetBuffer(pd.CulledPointLightIndices);
            auto& culledSpotLightIndicesBuffer  = context.GetBuffer(pd.CulledSpotLightIndices);

            PushConstantBlock pc = {.CameraDataBuffer                   = cameraDataBuffer->GetBDA(),
                                    .LightDataBuffer                    = lightDataBuffer->GetBDA(),
                                    .StorageImageIndex                  = lightHeatMapTexture->GetImage()->GetBindlessIndex(),
                                    .AlbedoTextureIndex                 = depthOpaqueTexture->GetBindlessIndex(),
                                    .LightCullingFrustumDataBuffer      = lightCullFrustumsDataBuffer->GetBDA(),
                                    .VisiblePointLightIndicesDataBuffer = culledPointLightIndicesBuffer->GetBDA(),
                                    .VisibleSpotLightIndicesDataBuffer  = culledSpotLightIndicesBuffer->GetBDA()};
            pc.data0.x           = frustumDebugTexture->GetImage()->GetBindlessIndex();

            const auto& pipeline = PipelineLibrary::Get(rd->LightCullingPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);

            cb->Dispatch(glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE), glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE));
        });
}

void LightCullingPass::AddComputeLightCullingFrustumsPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID LightCullFrustums;
    };

    rendergraph->AddPass<PassData>(
        "ComputeFrustums Pass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            const uint32_t adjustedTiledWidth  = glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE);
            const uint32_t adjustedTiledHeight = glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE);

            builder.DeclareBuffer("LightCullFrustums",
                                  {.DebugName  = "LightCullFrustums",
                                   .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS,
                                   .Capacity   = sizeof(TileFrustum) * adjustedTiledWidth * adjustedTiledHeight});
            pd.LightCullFrustums = builder.WriteBuffer("LightCullFrustums");

            pd.CameraData = builder.ReadBuffer("CameraData");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //    if (IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer        = context.GetBuffer(pd.CameraData);
            auto& lightCullFrustumsBuffer = context.GetBuffer(pd.LightCullFrustums);

            const PushConstantBlock pc = {.CameraDataBuffer              = cameraDataBuffer->GetBDA(),
                                          .LightCullingFrustumDataBuffer = lightCullFrustumsBuffer->GetBDA()};

            const auto& pipeline = PipelineLibrary::Get(rd->ComputeFrustumsPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);

            // Divide twice to make use of threads inside warps instead of creating frustum per warp.
            cb->Dispatch(glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE),
                         glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE));
        });
}

}  // namespace Pathfinder
