#include <PathfinderPCH.h>
#include "ObjectCulling.h"

#include <Renderer/Pipeline.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/RenderGraph/RenderGraph.h>
#include <Renderer/Renderer.h>
#include <Renderer/Mesh/Submesh.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

void ObjectCullingPass::AddPass(Unique<RenderGraph>& rendergraph)
{
    struct PassData
    {
        RGBufferID CameraData;
        RGBufferID MeshDataOpaque;
        RGBufferID MeshDataTransparent;
        RGBufferID DrawBufferOpaque;
        RGBufferID DrawBufferTransparent;
        RGBufferID CulledMeshesOpaque;
        RGBufferID CulledMeshesTransparent;
    };

    rendergraph->AddPass<PassData>(
        "ObjectCullingPass", ERGPassType::RGPASS_TYPE_COMPUTE,
        [=](PassData& pd, RenderGraphBuilder& builder)
        {
            pd.CameraData = builder.ReadBuffer("CameraData");

            pd.MeshDataOpaque          = builder.WriteBuffer("MeshDataOpaque_V1", "MeshDataOpaque_V0");
            pd.MeshDataTransparent     = builder.WriteBuffer("MeshDataTransparent_V1", "MeshDataTransparent_V0");
            pd.DrawBufferOpaque        = builder.WriteBuffer("DrawBufferOpaque_V1", "DrawBufferOpaque_V0");
            pd.DrawBufferTransparent   = builder.WriteBuffer("DrawBufferTransparent_V1", "DrawBufferTransparent_V0");
            pd.CulledMeshesOpaque      = builder.WriteBuffer("CulledMeshesOpaque_V1", "CulledMeshesOpaque_V0");
            pd.CulledMeshesTransparent = builder.WriteBuffer("CulledMeshesTransparent_V1", "CulledMeshesTransparent_V0");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //    if (IsWorldEmpty()) return;
            const auto& rd = Renderer::GetRendererData();

            auto& cameraDataBuffer         = context.GetBuffer(pd.CameraData);
            auto& meshDataOpaqueBuffer     = context.GetBuffer(pd.MeshDataOpaque);
            auto& drawBufferOpaque         = context.GetBuffer(pd.DrawBufferOpaque);
            auto& culledMeshesBufferOpaque = context.GetBuffer(pd.CulledMeshesOpaque);

            // 1. Opaque
            PushConstantBlock pc = {.CameraDataBuffer = cameraDataBuffer->GetBDA(),
                                    .addr0            = meshDataOpaqueBuffer->GetBDA(),
                                    .addr1            = drawBufferOpaque->GetBDA(),
                                    .addr2            = culledMeshesBufferOpaque->GetBDA()};
            pc.data0.x           = rd->OpaqueObjects.size();

            const auto& pipeline = PipelineLibrary::Get(rd->ObjectCullingPipelineHash);
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Dispatch(glm::ceil((float)rd->OpaqueObjects.size() / MESHLET_LOCAL_GROUP_SIZE));

            auto& meshesDataTransparentBuffer   = context.GetBuffer(pd.MeshDataTransparent);
            auto& drawBufferTransparent         = context.GetBuffer(pd.DrawBufferTransparent);
            auto& culledMeshesBufferTransparent = context.GetBuffer(pd.CulledMeshesTransparent);

            // 2. Transparent
            pc.data0.x = rd->TransparentObjects.size();
            pc.addr0   = meshesDataTransparentBuffer->GetBDA();
            pc.addr1   = drawBufferTransparent->GetBDA();
            pc.addr2   = culledMeshesBufferTransparent->GetBDA();
            Renderer::BindPipeline(cb, pipeline);
            cb->BindPushConstants(pipeline, 0, sizeof(pc), &pc);
            cb->Dispatch(glm::ceil((float)rd->TransparentObjects.size() / MESHLET_LOCAL_GROUP_SIZE));
        });
}

}  // namespace Pathfinder
