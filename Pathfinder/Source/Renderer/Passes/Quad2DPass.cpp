#include <PathfinderPCH.h>
#include "Quad2DPass.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

Quad2DPass::Quad2DPass(const uint32_t width, const uint32_t height, const uint64_t pipelineHash)
    : m_Width{width}, m_Height{height}, m_PipelineHash(pipelineHash)
{
}

void Quad2DPass::AddPass(Unique<RenderGraph>& rendergraph, std::vector<Sprite>& sprites, const uint32_t spriteCount)
{
    // Sort back to front.
    auto& rd = Renderer::GetRendererData();
    std::sort(std::execution::par, sprites.begin(), sprites.begin() + spriteCount,
              [&](const auto& lhs, const auto& rhs) -> bool
              {
                  const float lhsDist = glm::length(lhs.Translation - rd->CameraStruct.Position);
                  const float rhsDist = glm::length(rhs.Translation - rd->CameraStruct.Position);

                  return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
              });

    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID SpriteData;
    };

    rendergraph->AddPass<PassData>(
        "Quad2DPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.WriteDepthStencil("DepthOpaque_V2", {0.f, 0}, EOp::LOAD, EOp::STORE, EOp::DONT_CARE, EOp::DONT_CARE, "DepthOpaque_V1");
            builder.WriteRenderTarget("AlbedoTexture_V2", glm::vec4{0.f}, EOp::LOAD, EOp::STORE, "AlbedoTexture_V1");
            builder.WriteRenderTarget("HDRTexture_V2", glm::vec4{0.f}, EOp::LOAD, EOp::STORE, "HDRTexture_V1");

            pd.CameraData = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE);

            builder.DeclareBuffer("SpriteData", {.DebugName  = "SpriteData",
                                                 .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED | EBufferFlag::BUFFER_FLAG_ADDRESSABLE,
                                                 .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE});
            pd.SpriteData = builder.ReadBuffer("SpriteData", EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE |
                                                                 EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            // if (IsWorldEmpty()) return;

            auto& cameraDataBuffer = context.GetBuffer(pd.CameraData);
            auto& spriteDataBuffer = context.GetBuffer(pd.SpriteData);

            if (spriteCount == 0) return;

            if (const uint32_t dataSize = spriteCount * sizeof(Sprite); dataSize != 0) spriteDataBuffer->SetData(sprites.data(), dataSize);

            const PushConstantBlock pc = {.CameraDataBuffer = cameraDataBuffer->GetBDA(), .addr0 = spriteDataBuffer->GetBDA()};
            const auto& pipeline       = PipelineLibrary::Get(m_PipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Draw(6, spriteCount);
        });
}

}  // namespace Pathfinder
