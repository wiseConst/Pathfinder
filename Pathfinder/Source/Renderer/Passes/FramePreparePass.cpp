#include <PathfinderPCH.h>
#include "FramePreparePass.h"

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

void FramePreparePass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID LightData;
        RGBufferID CameraData;
        RGBufferID MeshDataOpaquePrepared;
        RGBufferID MeshDataTransparentPrepared;
        RGBufferID DrawBufferOpaquePrepared;
        RGBufferID DrawBufferTransparentPrepared;
        RGBufferID CulledMeshesOpaquePrepared;
        RGBufferID CulledMeshesTransparentPrepared;
    };

    rendergraph->AddPass<PassData>(
        "FramePrepare Pass", ERGPassType::RGPASS_TYPE_TRANSFER,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            RGBufferSpecification perFrameBS = {
                .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS, .bPerFrame = true};
            builder.DeclareBuffer("MeshDataOpaque_V0", perFrameBS);
            builder.DeclareBuffer("MeshDataTransparent_V0", perFrameBS);

            perFrameBS.Capacity = sizeof(LightData);
            builder.DeclareBuffer("LightData", perFrameBS);

            perFrameBS.Capacity = sizeof(CameraData);
            builder.DeclareBuffer("CameraData", perFrameBS);

            const RGBufferSpecification drawBufferBS = {
                .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                              EBufferUsage::BUFFER_USAGE_INDIRECT | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS,
                .bMapPersistent = true};

            builder.DeclareBuffer("DrawBufferOpaque_V0", drawBufferBS);
            builder.DeclareBuffer("DrawBufferTransparent_V0", drawBufferBS);

            const RGBufferSpecification culledMeshesBS = {.UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE |
                                                                        EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
            builder.DeclareBuffer("CulledMeshesOpaque_V0", culledMeshesBS);
            builder.DeclareBuffer("CulledMeshesTransparent_V0", culledMeshesBS);
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //  if (Renderer::IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& lightDataBuffer = context.GetBuffer(pd.LightData);
            lightDataBuffer->SetData(rd->LightStruct.get(), sizeof(LightData));

            auto& cameraDataBuffer = context.GetBuffer(pd.CameraData);
            cameraDataBuffer->SetData(&rd->CameraStruct, sizeof(rd->CameraStruct));

            // 1. Sort front to back, to minimize overdraw.
            std ::sort(std::execution::par, rd->OpaqueObjects.begin(), rd->OpaqueObjects.end(),
                       [&](const auto& lhs, const auto& rhs) -> bool
                       {
                           const float lhsDist = glm::length(lhs.Translation - rd->CameraStruct.Position);
                           const float rhsDist = glm::length(rhs.Translation - rd->CameraStruct.Position);

                           return lhsDist < rhsDist;  // Front to back drawing(minimize overdraw)
                       });

            // 2. Sort back to front.
            std::sort(std::execution::par, rd->TransparentObjects.begin(), rd->TransparentObjects.end(),
                      [&](const auto& lhs, const auto& rhs) -> bool
                      {
                          const float lhsDist = glm::length(lhs.Translation - rd->CameraStruct.Position);
                          const float rhsDist = glm::length(rhs.Translation - rd->CameraStruct.Position);

                          return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
                      });

            auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaquePrepared);
            auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaquePrepared);
            auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaquePrepared);

            meshDataOpaqueBuffer->Resize(rd->OpaqueObjects.size() * sizeof(MeshData));
            drawBufferOpaque->Resize(sizeof(uint32_t) + rd->OpaqueObjects.size() * sizeof(DrawMeshTasksIndirectCommand));
            culledMeshesBufferOpaque->Resize(rd->OpaqueObjects.size() * sizeof(uint32_t));

            std::vector<MeshData> opaqueMeshesData(rd->OpaqueObjects.size());
            for (uint32_t objIdx{}; objIdx < rd->OpaqueObjects.size(); ++objIdx)
            {
                const auto& submeshData     = rd->OpaqueObjects.at(objIdx);
                const auto& submesh         = submeshData.submesh;
                const uint32_t meshletCount = submesh->GetMeshletBuffer()->GetSpecification().Capacity / sizeof(Meshlet);
                opaqueMeshesData.at(objIdx) = {.sphere                    = submesh->GetBoundingSphere(),
                                               .meshletCount              = meshletCount,
                                               .translation               = submeshData.Translation,
                                               .scale                     = submeshData.Scale,
                                               .orientation               = submeshData.Orientation,
                                               .materialBufferBDA         = submesh->GetMaterial()->GetBDA(),
                                               .indexBufferBDA            = submesh->GetIndexBuffer()->GetBDA(),
                                               .vertexPosBufferBDA        = submesh->GetVertexPositionBuffer()->GetBDA(),
                                               .vertexAttributeBufferBDA  = submesh->GetVertexAttributeBuffer()->GetBDA(),
                                               .meshletBufferBDA          = submesh->GetMeshletBuffer()->GetBDA(),
                                               .meshletVerticesBufferBDA  = submesh->GetMeshletVerticesBuffer()->GetBDA(),
                                               .meshletTrianglesBufferBDA = submesh->GetMeshletTrianglesBuffer()->GetBDA()};
            }
            if (!opaqueMeshesData.empty())
                meshDataOpaqueBuffer->SetData(opaqueMeshesData.data(), opaqueMeshesData.size() * sizeof(opaqueMeshesData[0]));

            auto& meshDataTransparentBuffer     = context.GetBuffer(pd.MeshDataTransparentPrepared);
            auto& drawBufferTransparent         = context.GetBuffer(pd.DrawBufferTransparentPrepared);
            auto& culledMeshesBufferTransparent = context.GetBuffer(pd.CulledMeshesTransparentPrepared);

            meshDataTransparentBuffer->Resize(rd->TransparentObjects.size() * sizeof(MeshData));
            drawBufferTransparent->Resize(sizeof(uint32_t) + rd->TransparentObjects.size() * sizeof(DrawMeshTasksIndirectCommand));
            culledMeshesBufferTransparent->Resize(rd->TransparentObjects.size() * sizeof(uint32_t));

            std::vector<MeshData> transparentMeshesData = {};
            for (const auto& [submesh, translation, scale, orientation] : rd->TransparentObjects)
            {
                const uint32_t meshletCount = submesh->GetMeshletBuffer()->GetSpecification().Capacity / sizeof(Meshlet);
                transparentMeshesData.emplace_back(
                    submesh->GetBoundingSphere(), meshletCount, translation, scale, orientation, submesh->GetMaterial()->GetBDA(),
                    submesh->GetIndexBuffer()->GetBDA(), submesh->GetVertexPositionBuffer()->GetBDA(),
                    submesh->GetVertexAttributeBuffer()->GetBDA(), submesh->GetMeshletBuffer()->GetBDA(),
                    submesh->GetMeshletVerticesBuffer()->GetBDA(), submesh->GetMeshletTrianglesBuffer()->GetBDA());
            }
            if (!transparentMeshesData.empty())
                meshDataTransparentBuffer->SetData(transparentMeshesData.data(),
                                                   transparentMeshesData.size() * sizeof(transparentMeshesData[0]));

            if (drawBufferOpaque->GetMapped()) rd->ObjectCullStats.DrawCountOpaque = *(uint32_t*)drawBufferOpaque->GetMapped();
            if (drawBufferTransparent->GetMapped())
                rd->ObjectCullStats.DrawCountTransparent = *(uint32_t*)drawBufferTransparent->GetMapped();

            Renderer::GetStats().ObjectsDrawn = rd->ObjectCullStats.DrawCountOpaque + rd->ObjectCullStats.DrawCountTransparent;

            cb->FillBuffer(drawBufferOpaque, 0);
            cb->FillBuffer(culledMeshesBufferOpaque, 0);
            cb->FillBuffer(drawBufferTransparent, 0);
            cb->FillBuffer(culledMeshesBufferTransparent, 0);
        });
}

}  // namespace Pathfinder
