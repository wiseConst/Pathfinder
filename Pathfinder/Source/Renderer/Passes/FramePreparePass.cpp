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
    PrepareCSMData();

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
        RGBufferID CSMData;
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

            builder.DeclareBuffer("CSMData", {.DebugName  = "CSMData",
                                              .ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                              .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE});
            pd.CSMData = builder.WriteBuffer("CSMData");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //  if (Renderer::IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& csmDataBuffer = context.GetBuffer(pd.CSMData);
            csmDataBuffer->SetData(rd->ShadowMapData.data(), sizeof(rd->ShadowMapData));

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

glm::mat4 FramePreparePass::CalculateLightSpaceViewProjMatrix(const glm::vec3& lightDir, const float zNear, const float zFar)
{
    const auto& rd = Renderer::GetRendererData();

    // Converting our i-th camera's view frustum to world space, so we can tightly fit ortho frustum in the whole camera frustum.
    const glm::mat4 proj                    = glm::perspective(glm::radians(rd->CameraStruct.FOV),
                                                               rd->CameraStruct.FullResolution.x / rd->CameraStruct.FullResolution.y, zNear, zFar);
    std::vector<glm::vec4> frustumCornersWS = GetFrustumCornersWorldSpace(proj * rd->CameraStruct.View);

    glm::vec3 frustumCenter{0.f};
    for (const auto& frustumCorner : frustumCornersWS)
        frustumCenter += glm::vec3(frustumCorner);

    frustumCenter /= frustumCornersWS.size();
    const glm::mat4 lightView = glm::lookAt(frustumCenter + lightDir, frustumCenter, glm::vec3{0.f, 1.0f, 0.f});

    glm::vec3 minCoord{std::numeric_limits<float>::max()};
    glm::vec3 maxCoord{std::numeric_limits<float>::min()};
    for (const auto& frustumCorner : frustumCornersWS)
    {
        const auto cornerLightVS = glm::vec3(lightView * frustumCorner);
        minCoord                 = glm::min(minCoord, cornerLightVS);
        maxCoord                 = glm::max(maxCoord, cornerLightVS);
    }

#if 0
    // Think about it : not only geometry which is in the frustum can cast shadows on a surface in the frustum !
    // Pulling back and pushing away Z
    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
    if (minZ < 0)
    {
        minZ *= zMult;
    }
    else
    {
        minZ /= zMult;
    }
    if (maxZ < 0)
    {
        maxZ /= zMult;
    }
    else
    {
        maxZ *= zMult;
    }
#endif

    return glm::ortho(minCoord.x, maxCoord.x, minCoord.y, maxCoord.y, 0.0f, maxCoord.z - minCoord.z) * lightView;
}

std::vector<glm::vec4> FramePreparePass::GetFrustumCornersWorldSpace(const glm::mat4& projView)
{
    const auto inverseProjView = glm::inverse(projView);
    std::vector<glm::vec4> frustumCornersWS;
    for (uint32_t x{}; x < 2; ++x)
    {
        for (uint32_t y{}; y < 2; ++y)
        {
            for (uint32_t z{}; z < 2; ++z)
            {
                const glm::vec4 pt = inverseProjView * glm::vec4(x * 2.f - 1.f, y * 2.f - 1.f, (float)z, 1.f);
                frustumCornersWS.emplace_back(pt / pt.w);  // don't forget the perspective division
            }
        }
    }

    return frustumCornersWS;
}

void FramePreparePass::PrepareCSMData()
{
    const auto& rd = Renderer::GetRendererData();

    // NOTE: CascadePlacement should be in FrameData, now it's duplicated.
    for (size_t dirLightIndex{}; dirLightIndex < MAX_DIR_LIGHTS; ++dirLightIndex)
    {
        float divisor = 50.f;
        for (size_t cascadeIndex{}; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            rd->ShadowMapData[dirLightIndex].CascadePlacementZ[cascadeIndex] = rd->CameraStruct.zFar / divisor;
            divisor /= 2.f;
        }
    }

    /*m_CascadePlacementZ.front() = rd->CameraStruct.zNear;
    m_CascadePlacementZ.back()  = rd->CameraStruct.zFar;
    for (size_t i{}; i < m_CascadePlacementZ.size(); ++i)
    {
        if (i == 0 || i + 1 == m_CascadePlacementZ.size()) continue;

        m_CascadePlacementZ.at(i) = m_CascadePlacementZ.back() / (m_CascadePlacementZ.size() - i);
    }*/

    for (size_t dirLightIndex{}; dirLightIndex < MAX_DIR_LIGHTS; ++dirLightIndex)
    {
        if (!rd->LightStruct->DirectionalLights[dirLightIndex].bCastShadows) continue;

        for (size_t cascadeIndex{}; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            if (cascadeIndex + 1 == SHADOW_CASCADE_COUNT)
            {
                rd->ShadowMapData[dirLightIndex].ViewProj[cascadeIndex] =
                    CalculateLightSpaceViewProjMatrix(rd->LightStruct->DirectionalLights[dirLightIndex].Direction,
                                                      rd->ShadowMapData[dirLightIndex].CascadePlacementZ[cascadeIndex - 1],
                                                      rd->ShadowMapData[dirLightIndex].CascadePlacementZ[cascadeIndex]);
            }
            else
            {
                rd->ShadowMapData[dirLightIndex].ViewProj[cascadeIndex] =
                    CalculateLightSpaceViewProjMatrix(rd->LightStruct->DirectionalLights[dirLightIndex].Direction,
                                                      rd->ShadowMapData[dirLightIndex].CascadePlacementZ[cascadeIndex],
                                                      rd->ShadowMapData[dirLightIndex].CascadePlacementZ[cascadeIndex + 1]);
            }
        }
    }
}

}  // namespace Pathfinder
