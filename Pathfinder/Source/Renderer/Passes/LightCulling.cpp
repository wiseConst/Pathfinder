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
    AddComputeLightCullingFrustumsPass(rendergraph);
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
        "LightCullingPass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.CameraData        = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.LightData         = builder.ReadBuffer("LightData", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.LightCullFrustums = builder.ReadBuffer("LightCullFrustums", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
            pd.DepthOpaque       = builder.ReadTexture("DepthOpaque", EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);

            // TODO: Move  FrustumDebugTexture to ComputeFrustums pass
            builder.DeclareTexture("FrustumDebugTexture",
                                   {.DebugName = "FrustumDebugTexture",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT});
            pd.FrustumDebugTexture = builder.WriteTexture("FrustumDebugTexture");

            builder.DeclareTexture("LightHeatMapTexture",
                                   {.DebugName = "LightHeatMapTexture",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA8_UNORM,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT});
            pd.LightHeatMapTexture = builder.WriteTexture("LightHeatMapTexture");

            const uint32_t adjustedTiledWidth  = glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE);
            const uint32_t adjustedTiledHeight = glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE);

            const size_t cplibSize = MAX_POINT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;
            builder.DeclareBuffer("CulledPointLightIndices", {.DebugName  = "CulledPointLightIndices",
                                                              .ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                              .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
                                                              .Capacity   = cplibSize});
            pd.CulledPointLightIndices = builder.WriteBuffer("CulledPointLightIndices");

            const size_t csplibSize = MAX_SPOT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;
            builder.DeclareBuffer("CulledSpotLightIndices", {.DebugName  = "CulledSpotLightIndices",
                                                             .ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                             .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
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
                                    .StorageImageIndex                  = lightHeatMapTexture->GetStorageTextureBindlessIndex(),
                                    .AlbedoTextureIndex                 = depthOpaqueTexture->GetTextureBindlessIndex(),
                                    .LightCullingFrustumDataBuffer      = lightCullFrustumsDataBuffer->GetBDA(),
                                    .VisiblePointLightIndicesDataBuffer = culledPointLightIndicesBuffer->GetBDA(),
                                    .VisibleSpotLightIndicesDataBuffer  = culledSpotLightIndicesBuffer->GetBDA()};
            pc.data0.x           = frustumDebugTexture->GetStorageTextureBindlessIndex();

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
        "ComputeFrustumsPass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            const uint32_t adjustedTiledWidth  = glm::ceil((float)m_Width / LIGHT_CULLING_TILE_SIZE);
            const uint32_t adjustedTiledHeight = glm::ceil((float)m_Height / LIGHT_CULLING_TILE_SIZE);

            builder.DeclareBuffer("LightCullFrustums", {.DebugName  = "LightCullFrustums",
                                                        .ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                        .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
                                                        .Capacity   = sizeof(TileFrustum) * adjustedTiledWidth * adjustedTiledHeight});
            pd.LightCullFrustums = builder.WriteBuffer("LightCullFrustums");

            pd.CameraData = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_STORAGE_BUFFER |
                                                                 EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //    if (IsWorldEmpty()) return;

            if (!m_bRecomputeLightCullingFrustums) return;

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
