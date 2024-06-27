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
        RGBufferID MeshDataOpaque;
        RGBufferID MeshDataTransparent;
        RGBufferID DrawBufferOpaque;
        RGBufferID DrawBufferTransparent;
        RGBufferID CulledMeshesOpaque;
        RGBufferID CulledMeshesTransparent;
    };

    rendergraph->AddPass<PassData>(
        "FramePreparePass", ERGPassType::RGPASS_TYPE_TRANSFER,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            RGBufferSpecification perFrameBS = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL | EBufferFlag::BUFFER_FLAG_MAPPED,
                                                .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
                                                .bPerFrame  = true};

            perFrameBS.DebugName = "MeshDataOpaque_V0";
            builder.DeclareBuffer("MeshDataOpaque_V0", perFrameBS);
            pd.MeshDataOpaque = builder.WriteBuffer("MeshDataOpaque_V0");

            perFrameBS.DebugName = "MeshDataTransparent_V0";
            builder.DeclareBuffer("MeshDataTransparent_V0", perFrameBS);
            pd.MeshDataTransparent = builder.WriteBuffer("MeshDataTransparent_V0");

            perFrameBS.Capacity  = sizeof(LightData);
            perFrameBS.DebugName = "LightData";
            builder.DeclareBuffer("LightData", perFrameBS);
            pd.LightData = builder.WriteBuffer("LightData");

            perFrameBS.Capacity  = sizeof(CameraData);
            perFrameBS.DebugName = "CameraData";
            builder.DeclareBuffer("CameraData", perFrameBS);
            pd.CameraData = builder.WriteBuffer("CameraData");

            RGBufferSpecification drawBufferBS = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_ADDRESSABLE | EBufferFlag::BUFFER_FLAG_MAPPED,
                                                  .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_INDIRECT};

            drawBufferBS.DebugName = "DrawBufferOpaque_V0";
            builder.DeclareBuffer("DrawBufferOpaque_V0", drawBufferBS);
            pd.DrawBufferOpaque = builder.WriteBuffer("DrawBufferOpaque_V0");

            drawBufferBS.DebugName = "DrawBufferTransparent_V0";
            builder.DeclareBuffer("DrawBufferTransparent_V0", drawBufferBS);
            pd.DrawBufferTransparent = builder.WriteBuffer("DrawBufferTransparent_V0");

            RGBufferSpecification culledMeshesBS = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                    .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE};

            culledMeshesBS.DebugName = "CulledMeshesOpaque_V0";
            builder.DeclareBuffer("CulledMeshesOpaque_V0", culledMeshesBS);
            pd.CulledMeshesOpaque = builder.WriteBuffer("CulledMeshesOpaque_V0");

            culledMeshesBS.DebugName = "CulledMeshesTransparent_V0";
            builder.DeclareBuffer("CulledMeshesTransparent_V0", culledMeshesBS);
            pd.CulledMeshesTransparent = builder.WriteBuffer("CulledMeshesTransparent_V0");
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

            auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaque);
            auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaque);
            auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaque);

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

            auto& meshDataTransparentBuffer     = context.GetBuffer(pd.MeshDataTransparent);
            auto& drawBufferTransparent         = context.GetBuffer(pd.DrawBufferTransparent);
            auto& culledMeshesBufferTransparent = context.GetBuffer(pd.CulledMeshesTransparent);

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


            cb->FillBuffer(drawBufferOpaque, 0);
            cb->FillBuffer(culledMeshesBufferOpaque, 0);
            cb->FillBuffer(drawBufferTransparent, 0);
            cb->FillBuffer(culledMeshesBufferTransparent, 0);
        });
}

}  // namespace Pathfinder
