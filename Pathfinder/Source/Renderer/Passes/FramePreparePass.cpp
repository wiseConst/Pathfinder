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
                                              .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE,
                                              .bPerFrame  = true});
            pd.CSMData = builder.WriteBuffer("CSMData");
        },
        [=](const PassData& pd, RenderGraphContext& context, Shared<CommandBuffer>& cb)
        {
            //  if (Renderer::IsWorldEmpty()) return;

            const auto& rd = Renderer::GetRendererData();

            auto& csmDataBuffer = context.GetBuffer(pd.CSMData);
            csmDataBuffer->SetData(&rd->ShadowMapData, sizeof(rd->ShadowMapData));

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

void FramePreparePass::PrepareCSMData()
{
    const auto& rd = Renderer::GetRendererData();

    const float nearClip  = rd->CameraStruct.zNear;
    const float farClip   = rd->CameraStruct.zFar;
    const float clipRange = farClip - nearClip;

    const float zMin = nearClip;
    const float zMax = zMin + clipRange;

    const float ratio = zMax / zMin;
    const float range = zMax - zMin;

    // Calculate split depths based on view camera frustum using NVidia's approach.
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    for (size_t i{}; i < SHADOW_CASCADE_COUNT; ++i)
    {
        const float p           = (i + 1) / static_cast<float>(SHADOW_CASCADE_COUNT);  // i/m in nvidia formula
        const float logPart     = zMin * glm::pow(ratio, p);
        const float uniformPart = zMin + range * p;

        const float d = uniformPart + rd->ShadowMapData.SplitLambda * (logPart - uniformPart);
        // rd->ShadowMapData.SplitLambda * logPart + (1.f - rd->ShadowMapData.SplitLambda) * uniformPart;
        rd->ShadowMapData.CascadePlacementZ[i] = (d - nearClip) / clipRange;  // d;
    };

    const auto invViewProj =
        glm::inverse(glm::perspective(glm::radians(rd->CameraStruct.FOV),
                                      rd->CameraStruct.FullResolution.x / rd->CameraStruct.FullResolution.y, nearClip, farClip) *
                     rd->CameraStruct.View);
    // Calculate orthographic projection matrix for each cascade
    for (size_t dirLightIndex{}; dirLightIndex < MAX_DIR_LIGHTS; ++dirLightIndex)
    {
        if (!rd->LightStruct->DirectionalLights[dirLightIndex].bCastShadows) continue;

        float lastSplitDist{0.f};
        const auto normalizedLightDir = glm::normalize(rd->LightStruct->DirectionalLights[dirLightIndex].Direction);
        for (size_t cascadeIndex{}; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
        {
            const float splitDist = rd->ShadowMapData.CascadePlacementZ[cascadeIndex];

            // Initially frustums corners are in NDC, depth range [0, 1].
            std::array<glm::vec3, 8> frustumCorners = {
                glm::vec3(-1.0f, 1.0f, 0.0f),   // Near top left
                glm::vec3(1.0f, 1.0f, 0.0f),    // Near top right
                glm::vec3(1.0f, -1.0f, 0.0f),   // Near bottom right
                glm::vec3(-1.0f, -1.0f, 0.0f),  // Near bottom left
                glm::vec3(-1.0f, 1.0f, 1.0f),   // Far top left
                glm::vec3(1.0f, 1.0f, 1.0f),    // Far top right
                glm::vec3(1.0f, -1.0f, 1.0f),   // Far bottom right
                glm::vec3(-1.0f, -1.0f, 1.0f)   // Far bottom left
            };

            // Transform frustum corners to world space.
            for (auto& frustumCorner : frustumCorners)
            {
                const glm::vec4 invCorner = invViewProj * glm::vec4(frustumCorner, 1.0f);
                frustumCorner             = invCorner / invCorner.w;
            }

            // Adjusting frustum sides:
            // Once we have our corners we can create a ray between the near and corresponding far corner,
            // normalize it and then multiply it by the new length and then our previous length becomes
            // the starting point for the next partition. Then we get the longest radius of this slice
            // and use it as the basis for our AABB.
            for (uint32_t i = 0; i < 4; ++i)
            {
                const glm::vec3 cornerRay = frustumCorners[i + 4] - frustumCorners[i];

                const glm::vec3 nearCornerRay = cornerRay * lastSplitDist;
                frustumCorners[i]             = frustumCorners[i] + nearCornerRay;

                const glm::vec3 farCornerRay = cornerRay * splitDist;
                frustumCorners[i + 4]        = frustumCorners[i] + farCornerRay;
            }

            // Get averaged frustum center
            glm::vec3 frustumCenter{0.f};
            for (auto& frustumCorner : frustumCorners)
            {
                frustumCenter += frustumCorner;
            }
            frustumCenter /= frustumCorners.size();

            float radius{0.f};
            for (auto& frustumCorner : frustumCorners)
            {
                const float distance = glm::length(frustumCorner - frustumCenter);
                radius               = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 maxExtents{radius};
            const glm::vec3 minExtents{-maxExtents};

            const glm::mat4 lightView =
                glm::lookAt(frustumCenter + normalizedLightDir * -minExtents.z, frustumCenter, glm::vec3{0.f, 1.f, 0.f});
            const glm::mat4 lightOrthoProj =
                glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.f, maxExtents.z - minExtents.z);

            // In order to avoid light shimmering we need to create a rounding matrix so we move in texel sized increments.
            // You can think of it as finding out how much we need to move the orthographic matrix so it matches up with shadow map,
            // it is done like this:
            glm::mat4 shadowMatrix   = lightOrthoProj * lightView;
            glm::vec4 shadowOrigin   = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin             = shadowMatrix * shadowOrigin;
            const float shadowMapDim = static_cast<float>(Renderer::GetRendererSettings().ShadowMapDim.x);
            shadowOrigin             = shadowOrigin * shadowMapDim / 2.0f;

            glm::vec4 roundedOrigin = glm::round(shadowOrigin);
            glm::vec4 roundOffset   = roundedOrigin - shadowOrigin;
            roundOffset             = roundOffset * 2.0f / shadowMapDim;
            roundOffset.z           = 0.0f;
            roundOffset.w           = 0.0f;

            glm::mat4 shadowProj = lightOrthoProj;
            shadowProj[3] += roundOffset;
            shadowMatrix = shadowProj * lightView;

            rd->ShadowMapData.CascadeData[dirLightIndex].ViewProj[cascadeIndex] = shadowMatrix;

            // Since in view space cam points towards -Z
            rd->ShadowMapData.CascadePlacementZ[cascadeIndex] = -(nearClip + splitDist * clipRange);
            lastSplitDist                                     = splitDist;
        }
    }
}

}  // namespace Pathfinder
