#include <PathfinderPCH.h>
#include "GBufferPass.h"

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

GBufferPass::GBufferPass(const uint32_t width, const uint32_t height) : m_Width{width}, m_Height{height} {}

void GBufferPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    AddForwardPlusOpaquePass(rendergraph);
    AddForwardPlusTransparentPass(rendergraph);
}

void GBufferPass::AddForwardPlusOpaquePass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CSMData;
        RGBufferID CameraData;
        RGBufferID LightData;
        RGBufferID MeshData;
        RGBufferID CulledMeshes;
        RGBufferID DrawBuffer;
        RGBufferID CulledPointLightIndices;
        RGBufferID CulledSpotLightIndices;
        RGTextureID AOBlurTexture;
        RGTextureID SSSTexture;
    };

    rendergraph->AddPass<PassData>(
        "ForwardPlusOpaquePass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.WriteDepthStencil("DepthOpaque_V0", {0.f, 0}, EOp::LOAD, EOp::STORE, EOp::DONT_CARE, EOp::DONT_CARE, "DepthOpaque");

            builder.DeclareTexture("AlbedoTexture_V0",
                                   {.DebugName  = "AlbedoTexture_V0",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA16F,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("AlbedoTexture_V0", glm::vec4{0.f}, EOp::CLEAR, EOp::STORE);

            builder.DeclareTexture("HDRTexture_V0",
                                   {.DebugName  = "HDRTexture_V0",
                                    .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                    .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                    .MinFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .MagFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                    .Format     = EImageFormat::FORMAT_RGBA16F,
                                    .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
            builder.WriteRenderTarget("HDRTexture_V0", glm::vec4{0.f}, EOp::CLEAR, EOp::STORE);

            pd.CameraData   = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.LightData    = builder.ReadBuffer("LightData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.MeshData     = builder.ReadBuffer("MeshDataOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
            pd.CulledMeshes = builder.ReadBuffer("CulledMeshesOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
            pd.DrawBuffer   = builder.ReadBuffer("DrawBufferOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE |
                                                                            EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT);
            pd.CulledPointLightIndices =
                builder.ReadBuffer("CulledPointLightIndices", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.CulledSpotLightIndices =
                builder.ReadBuffer("CulledSpotLightIndices", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.AOBlurTexture = builder.ReadTexture("AOBlurTexture", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.SSSTexture    = builder.ReadTexture("SSSTexture", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);

            const auto& rd = Renderer::GetRendererData();

            pd.CSMData = builder.ReadBuffer("CSMData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            for (size_t dirLightIndex{}; dirLightIndex < rd->LightStruct->DirectionalLightCount; ++dirLightIndex)
            {
                if (!rd->LightStruct->DirectionalLights[dirLightIndex].bCastShadows) continue;

                for (uint32_t cascadeIndex{}; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
                {
                    const std::string passName =
                        "DirLight[" + std::to_string(dirLightIndex) + "]_CSMPass[" + std::to_string(cascadeIndex) + "]";

                    const std::string csmTextureName = "Cascade[" + std::to_string(cascadeIndex) + "]";
                    builder.ReadTexture(csmTextureName, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
                }
            }
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //   if (IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer              = context.GetBuffer(pd.CameraData);
            auto& lightDataBuffer               = context.GetBuffer(pd.LightData);
            auto& meshDataOpaqueBuffer          = context.GetBuffer(pd.MeshData);
            auto& drawBufferOpaque              = context.GetBuffer(pd.DrawBuffer);
            auto& culledMeshesBufferOpaque      = context.GetBuffer(pd.CulledMeshes);
            auto& culledPointLightIndicesBuffer = context.GetBuffer(pd.CulledPointLightIndices);
            auto& culledSpotLightIndicesBuffer  = context.GetBuffer(pd.CulledSpotLightIndices);
            auto& aoBlurTexture                 = context.GetTexture(pd.AOBlurTexture);
            auto& sssTexture                    = context.GetTexture(pd.SSSTexture);  // TODO: use it
            auto& csmDataBuffer                 = context.GetBuffer(pd.CSMData);

            if (drawBufferOpaque->GetMapped()) rd->ObjectCullStats.DrawCountOpaque = *(uint32_t*)drawBufferOpaque->GetMapped();
            Renderer::GetStats().ObjectsDrawn += rd->ObjectCullStats.DrawCountOpaque;

            const PushConstantBlock pc = {.CameraDataBuffer                   = cameraDataBuffer->GetBDA(),
                                          .LightDataBuffer                    = lightDataBuffer->GetBDA(),
                                          .StorageImageIndex                  = aoBlurTexture->GetTextureBindlessIndex(),
                                          .VisiblePointLightIndicesDataBuffer = culledPointLightIndicesBuffer->GetBDA(),
                                          .VisibleSpotLightIndicesDataBuffer  = culledSpotLightIndicesBuffer->GetBDA(),
                                          .addr0                              = meshDataOpaqueBuffer->GetBDA(),
                                          .addr1                              = culledMeshesBufferOpaque->GetBDA(),
                                          .addr2                              = csmDataBuffer->GetBDA()};

            const auto& pipeline = PipelineLibrary::Get(rd->ForwardPlusOpaquePipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->DrawMeshTasksMultiIndirect(drawBufferOpaque, sizeof(uint32_t), drawBufferOpaque, 0,
                                           (drawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) /
                                               sizeof(DrawMeshTasksIndirectCommand),
                                           sizeof(DrawMeshTasksIndirectCommand));
        });
}

void GBufferPass::AddForwardPlusTransparentPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID LightData;
        RGBufferID MeshData;
        RGBufferID CulledMeshes;
        RGBufferID DrawBuffer;
        RGBufferID CulledPointLightIndices;
        RGBufferID CulledSpotLightIndices;
        RGTextureID AOBlurTexture;
        RGTextureID SSSTexture;
    };

    rendergraph->AddPass<PassData>(
        "ForwardPlusTransparentPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.WriteDepthStencil("DepthOpaque_V1", {0.f, 0}, EOp::LOAD, EOp::STORE, EOp::DONT_CARE, EOp::DONT_CARE, "DepthOpaque_V0");
            builder.WriteRenderTarget("AlbedoTexture_V1", glm::vec4{0.f}, EOp::LOAD, EOp::STORE, "AlbedoTexture_V0");
            builder.WriteRenderTarget("HDRTexture_V1", glm::vec4{0.f}, EOp::LOAD, EOp::STORE, "HDRTexture_V0");

            pd.CameraData   = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.LightData    = builder.ReadBuffer("LightData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.MeshData     = builder.ReadBuffer("MeshDataTransparent_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
            pd.CulledMeshes = builder.ReadBuffer("CulledMeshesTransparent_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
            pd.DrawBuffer   = builder.ReadBuffer("DrawBufferTransparent_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE |
                                                                                 EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT);
            pd.CulledPointLightIndices =
                builder.ReadBuffer("CulledPointLightIndices", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.CulledSpotLightIndices =
                builder.ReadBuffer("CulledSpotLightIndices", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.AOBlurTexture = builder.ReadTexture("AOBlurTexture", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);
            pd.SSSTexture    = builder.ReadTexture("SSSTexture", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //   if (IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer              = context.GetBuffer(pd.CameraData);
            auto& lightDataBuffer               = context.GetBuffer(pd.LightData);
            auto& meshDataTransparentBuffer     = context.GetBuffer(pd.MeshData);
            auto& drawBufferTransparent         = context.GetBuffer(pd.DrawBuffer);
            auto& culledMeshesBufferTransparent = context.GetBuffer(pd.CulledMeshes);
            auto& culledPointLightIndicesBuffer = context.GetBuffer(pd.CulledPointLightIndices);
            auto& culledSpotLightIndicesBuffer  = context.GetBuffer(pd.CulledSpotLightIndices);
            auto& aoBlurTexture                 = context.GetTexture(pd.AOBlurTexture);
            auto& sssTexture                    = context.GetTexture(pd.SSSTexture);  // TODO: use it

            if (drawBufferTransparent->GetMapped())
                rd->ObjectCullStats.DrawCountTransparent = *(uint32_t*)drawBufferTransparent->GetMapped();
            Renderer::GetStats().ObjectsDrawn += rd->ObjectCullStats.DrawCountTransparent;

            const PushConstantBlock pc = {.CameraDataBuffer                   = cameraDataBuffer->GetBDA(),
                                          .LightDataBuffer                    = lightDataBuffer->GetBDA(),
                                          .StorageImageIndex                  = aoBlurTexture->GetTextureBindlessIndex(),
                                          .VisiblePointLightIndicesDataBuffer = culledPointLightIndicesBuffer->GetBDA(),
                                          .VisibleSpotLightIndicesDataBuffer  = culledSpotLightIndicesBuffer->GetBDA(),
                                          .addr0                              = meshDataTransparentBuffer->GetBDA(),
                                          .addr1                              = culledMeshesBufferTransparent->GetBDA()};

            const auto& pipeline = PipelineLibrary::Get(rd->ForwardPlusTransparentPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->DrawMeshTasksMultiIndirect(drawBufferTransparent, sizeof(uint32_t), drawBufferTransparent, 0,
                                           (drawBufferTransparent->GetSpecification().Capacity - sizeof(uint32_t)) /
                                               sizeof(DrawMeshTasksIndirectCommand),
                                           sizeof(DrawMeshTasksIndirectCommand));
        });
}

}  // namespace Pathfinder
