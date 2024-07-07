#include <PathfinderPCH.h>
#include "CascadedShadowMapPass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Mesh/Submesh.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

CascadedShadowMapPass::CascadedShadowMapPass(const uint32_t width, const uint32_t height)
    : m_Width{std::max(width, height)}, m_Height{std::max(width, height)}
{
}

void CascadedShadowMapPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    const auto& rd = Renderer::GetRendererData();

    struct PassData
    {
        RGBufferID CSMData;
        RGBufferID MeshDataOpaque;
        RGBufferID CulledMeshesOpaque;
        RGBufferID DrawBufferOpaque;
        RGTextureID ShadowMapTexture;
    };

    for (size_t dirLightIndex{}; dirLightIndex < rd->LightStruct->DirectionalLightCount; ++dirLightIndex)
    {
        if (!rd->LightStruct->DirectionalLights[dirLightIndex].bCastShadows) continue;

        for (uint32_t cascadeIndex{}; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            const std::string passName = "DirLight[" + std::to_string(dirLightIndex) + "]_CSMPass[" + std::to_string(cascadeIndex) + "]";

            rendergraph->AddPass<PassData>(
                passName, ERGPassType::RGPASS_TYPE_GRAPHICS,
                [=](PassData& pd, RenderGraphBuilder& builder)
                {
                    const std::string csmTextureName = "Cascade[" + std::to_string(cascadeIndex) + "]";
                    builder.DeclareTexture(csmTextureName, {.DebugName  = csmTextureName,
                                                            .Dimensions = glm::uvec3(m_Width, m_Height, 1),
                                                            .WrapS      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                            .WrapT      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                            .MinFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                                            .MagFilter  = ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                                            .Format     = EImageFormat::FORMAT_D16_UNORM,
                                                            .UsageFlags = EImageUsage::IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                                          EImageUsage::IMAGE_USAGE_SAMPLED_BIT});
                    pd.ShadowMapTexture =
                        builder.WriteDepthStencil(csmTextureName, DepthStencilClearValue{1.0f, 0}, EOp::CLEAR, EOp::STORE);

                    pd.MeshDataOpaque = builder.ReadBuffer("MeshDataOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
                    pd.CulledMeshesOpaque =
                        builder.ReadBuffer("CulledMeshesOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);
                    pd.DrawBufferOpaque = builder.ReadBuffer("DrawBufferOpaque_V1", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE |
                                                                                        EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT);

                    pd.CSMData = builder.ReadBuffer("CSMData", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

                    builder.SetViewportScissor(m_Width, m_Height);
                },
                [=, &rd](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
                {
                    //   if (IsWorldEmpty()) return;
                    auto& csmDataBuffer            = context.GetBuffer(pd.CSMData);
                    auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaque);
                    auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaque);
                    auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaque);

                    rd->ShadowMapData[dirLightIndex].CascadeTextureIndices[cascadeIndex] =
                        context.GetTexture(pd.ShadowMapTexture)->GetTextureBindlessIndex();
                    csmDataBuffer->SetData(rd->ShadowMapData.data(), sizeof(rd->ShadowMapData));

                    const PushConstantBlock pc = {
                        .StorageImageIndex  = static_cast<const uint32_t>(dirLightIndex),
                        .AlbedoTextureIndex = cascadeIndex,
                        .addr0              = meshDataOpaqueBuffer->GetBDA(),
                        .addr1              = culledMeshesBufferOpaque->GetBDA(),
                        .addr2              = csmDataBuffer->GetBDA(),
                    };

                    const auto& pipeline = PipelineLibrary::Get(rd->CSMPipelineHash);
                    Renderer::BindPipeline(cb, pipeline);
                    cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
                    cb->DrawMeshTasksMultiIndirect(drawBufferOpaque, sizeof(uint32_t), drawBufferOpaque, 0,
                                                   (drawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) /
                                                       sizeof(DrawMeshTasksIndirectCommand),
                                                   sizeof(DrawMeshTasksIndirectCommand));
                });
        }
    }
}

}  // namespace Pathfinder
