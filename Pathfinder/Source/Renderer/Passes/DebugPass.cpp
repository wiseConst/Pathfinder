#include <PathfinderPCH.h>
#include "DebugPass.h"

#include <Renderer/Debug/DebugRenderer.h>
#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>

#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

#include <Renderer/Mesh/MeshManager.h>

namespace Pathfinder
{

void DebugPass::AddPass(Unique<RenderGraph>& rendergraph, const LineVertex* lines, const uint32_t lineVertexCount,
                        const std::vector<DebugSphereData>& debugSpheres)
{
    if (!m_DebugSphereMesh.IndexBuffer || !m_DebugSphereMesh.VertexBuffer) m_DebugSphereMesh = MeshManager::GenerateUVSphere(24, 18);

    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID LineVertexBuffer;
        RGBufferID DebugSphereData;
    };

    rendergraph->AddPass<PassData>(
        "DebugPass", ERGPassType::RGPASS_TYPE_GRAPHICS,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            builder.WriteDepthStencil("DepthOpaque_V3", {0.f, 0}, EOp::LOAD, EOp::STORE, EOp::DONT_CARE, EOp::DONT_CARE, "DepthOpaque_V2");
            builder.WriteRenderTarget("AlbedoTexture_V3", glm::vec4{0.f}, EOp::LOAD, EOp::STORE, "AlbedoTexture_V2");

            pd.CameraData = builder.ReadBuffer("CameraData", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

            builder.DeclareBuffer("LineVertexBuffer", {.DebugName  = "LineVertexBuffer",
                                                       .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED,
                                                       .UsageFlags = EBufferUsage::BUFFER_USAGE_VERTEX,
                                                       .bPerFrame  = true});
            pd.LineVertexBuffer = builder.ReadBuffer("LineVertexBuffer", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

            builder.DeclareBuffer("DebugSphereData", {.DebugName  = "DebugSphereData",
                                                      .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED | EBufferFlag::BUFFER_FLAG_ADDRESSABLE,
                                                      .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
                                                      .bPerFrame  = true});
            pd.DebugSphereData = builder.ReadBuffer("DebugSphereData", EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE);

            builder.SetViewportScissor(m_Width, m_Height);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            // if (IsWorldEmpty()) return;

            auto& cameraDataBuffer      = context.GetBuffer(pd.CameraData);
            auto& lineVertexBuffer      = context.GetBuffer(pd.LineVertexBuffer);
            auto& debugSphereDataBuffer = context.GetBuffer(pd.DebugSphereData);

            if (const auto lineDataSize = lineVertexCount * sizeof(LineVertex); lineDataSize > 0)
            {
                lineVertexBuffer->SetData(lines, lineDataSize);

                cb->BeginDebugLabel("DebugLines", glm::vec3(.1f, .8f, .3f));

                const PushConstantBlock pc = {.CameraDataBuffer = cameraDataBuffer->GetBDA()};
                constexpr uint64_t offset  = 0;
                cb->BindVertexBuffers({lineVertexBuffer}, 0, 1, &offset);

                const auto& pipeline = PipelineLibrary::Get(m_LinePipelineHash);
                Renderer::BindPipeline(cb, pipeline);

                cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
                cb->Draw(lineVertexCount);

                cb->EndDebugLabel();
            }

            if (!debugSpheres.empty())
            {
                debugSphereDataBuffer->SetData(debugSpheres.data(), debugSpheres.size() * sizeof(DebugSphereData));

                cb->BeginDebugLabel("DebugSpheres", glm::vec3(.1f, .3f, .8f));

                const auto& pipeline = PipelineLibrary::Get(m_SpherePipelineHash);
                Renderer::BindPipeline(cb, pipeline);
                const PushConstantBlock pc = {.CameraDataBuffer = cameraDataBuffer->GetBDA(), .addr0 = debugSphereDataBuffer->GetBDA()};
                cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);

                constexpr uint64_t offset = 0;
                cb->BindVertexBuffers({m_DebugSphereMesh.VertexBuffer}, 0, 1, &offset);
                cb->BindIndexBuffer(m_DebugSphereMesh.IndexBuffer);

                cb->DrawIndexed(m_DebugSphereMesh.IndexBuffer->GetSpecification().Capacity / sizeof(uint32_t), debugSpheres.size());

                cb->EndDebugLabel();
            }
        });
}

}  // namespace Pathfinder
