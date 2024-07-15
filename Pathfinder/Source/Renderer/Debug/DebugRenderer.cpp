#include <PathfinderPCH.h>
#include "DebugRenderer.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Renderer.h>
#include <Renderer/Pipeline.h>
#include <Renderer/Shader.h>
#include <Renderer/Buffer.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/Mesh/Mesh.h>
#include <Renderer/Mesh/Submesh.h>

#include <Renderer/RenderGraph/RenderGraph.h>

namespace Pathfinder
{
static bool s_bDebugRendererInit = false;
static bool s_bStateNeedsUpdate  = true;

#define UPDATE_STATE_ROUTINE                                                                                                               \
    {                                                                                                                                      \
        if (!s_bDebugRendererInit) return;                                                                                                 \
        UpdateState();                                                                                                                     \
    }

void DebugRenderer::Init()
{
    PFR_ASSERT(!s_bDebugRendererInit, "DebugRenderer already initialized!");
    s_DebugRendererData             = MakeUnique<DebugRendererData>();
    s_DebugRendererData->FrameIndex = 0;

    ShaderLibrary::Load({{"Debug/Line"}, {"Debug/Sphere"}});
    ShaderLibrary::WaitUntilShadersLoaded();

    std::ranges::for_each(s_DebugRendererData->LineVertexBase,
                          [](auto& lineVertexBase)
                          {
                              // TODO: Use here CPU-side memory allocator
                              lineVertexBase = new LineVertex[s_MAX_VERTICES];
                          });

    for (uint32_t fif{}; fif < s_FRAMES_IN_FLIGHT; ++fif)
    {
        s_DebugRendererData->LineVertexCurrent[fif] = s_DebugRendererData->LineVertexBase[fif];
    }

    // NOTE: Formats should be the same as ForwardPlus pipeline, but excluding HDR format.
    const GraphicsPipelineOptions sphereGPO = {
        .VertexStreams     = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}},
        .Formats           = {EImageFormat::FORMAT_RGBA16F, EImageFormat::FORMAT_D32F},
        .FrontFace         = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
        .PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .LineWidth         = s_DebugRendererData->LineWidth,
        .bBlendEnable      = true,
        .BlendMode         = EBlendMode::BLEND_MODE_ALPHA,
        .PolygonMode       = EPolygonMode::POLYGON_MODE_LINE,
        .bDepthTest        = true,
        .bDepthWrite       = false,
        .DepthCompareOp    = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};
    PipelineSpecification spherePipelineSpec = {.DebugName       = "DebugSphere",
                                                .PipelineOptions = sphereGPO,
                                                .Shader          = ShaderLibrary::Get("Debug/Sphere"),
                                                .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    s_DebugRendererData->SpherePipelineHash  = PipelineLibrary::Push(spherePipelineSpec);

    // NOTE: Formats should be the same as ForwardPlus pipeline, but excluding HDR format.
    const GraphicsPipelineOptions lineGPO  = {.VertexStreams = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                                                 {"inColor", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT}}},
                                              .Formats       = {EImageFormat::FORMAT_RGBA16F, EImageFormat::FORMAT_D32F},
                                              .FrontFace     = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
                                              .PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_LIST,
                                              .LineWidth         = s_DebugRendererData->LineWidth,
                                              .bBlendEnable      = true,
                                              .BlendMode         = EBlendMode::BLEND_MODE_ALPHA,
                                              .PolygonMode       = EPolygonMode::POLYGON_MODE_LINE,
                                              .bDepthTest        = true,
                                              .bDepthWrite       = false,
                                              .DepthCompareOp    = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};
    PipelineSpecification linePipelineSpec = {.DebugName       = "DebugLine",
                                              .PipelineOptions = lineGPO,
                                              .Shader          = ShaderLibrary::Get("Debug/Line"),
                                              .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    s_DebugRendererData->LinePipelineHash  = PipelineLibrary::Push(linePipelineSpec);

    const auto& windowSpec = Application::Get().GetWindow()->GetSpecification();
    s_DebugRendererData->DebugPass =
        DebugPass(windowSpec.Width, windowSpec.Height, s_DebugRendererData->SpherePipelineHash, s_DebugRendererData->LinePipelineHash);

    Application::Get().GetWindow()->AddResizeCallback(
        [](const WindowResizeData& resizeData)
        { s_DebugRendererData->DebugPass.OnResize(resizeData.Dimensions.x, resizeData.Dimensions.y); });

    LOG_TRACE("{}", __FUNCTION__);
    s_bDebugRendererInit = true;
}

void DebugRenderer::Shutdown()
{
    if (s_bDebugRendererInit)
    {
        s_DebugRendererData.reset();
        s_bDebugRendererInit = false;
        LOG_TRACE("{}", __FUNCTION__);
    }
}

void DebugRenderer::UpdateState()
{
    if (!s_bDebugRendererInit || !s_bStateNeedsUpdate) return;

    memset(s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], 0,
           s_DebugRendererData->LineVertexCount * sizeof(LineVertex));
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
        s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
    s_DebugRendererData->LineVertexCount = 0;

    s_DebugRendererData->DebugSpheres.clear();

    auto& rd = Renderer::GetRendererData();
    if (s_DebugRendererData->FrameIndex == rd->FrameIndex) return;

    s_DebugRendererData->FrameIndex = rd->FrameIndex;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
        s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];

    s_bStateNeedsUpdate = false;
}

void DebugRenderer::Flush(Unique<RenderGraph>& renderGraph)
{
    UPDATE_STATE_ROUTINE;

    s_DebugRendererData->DebugPass.AddPass(renderGraph, s_DebugRendererData->LineVertexBase.at(s_DebugRendererData->FrameIndex),
                                           s_DebugRendererData->LineVertexCount, s_DebugRendererData->DebugSpheres);

    s_bStateNeedsUpdate = true;
}

void DebugRenderer::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Position = p0;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Color    = glm::packUnorm4x8(color);
    ++s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex];

    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Position = p1;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Color    = glm::packUnorm4x8(color);
    ++s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex];

    s_DebugRendererData->LineVertexCount += 2;
}

void DebugRenderer::DrawRect(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    const glm::vec3& p0 = glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    const glm::vec3& p1 = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    const glm::vec3& p2 = glm::vec3(center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z);
    const glm::vec3& p3 = glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z);

    DrawLine(p0, p1, color);
    DrawLine(p1, p2, color);
    DrawLine(p2, p3, color);
    DrawLine(p3, p0, color);
}

void DebugRenderer::DrawRect(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    glm::vec3 lineVertices[4] = {glm::vec3(center + halfExtents),
                                 glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z),
                                 glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z),
                                 glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z)};
    for (size_t i{}; i < 4; ++i)
        lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

    DrawLine(lineVertices[0], lineVertices[1], color);
    DrawLine(lineVertices[1], lineVertices[2], color);
    DrawLine(lineVertices[2], lineVertices[3], color);
    DrawLine(lineVertices[3], lineVertices[0], color);
}

void DebugRenderer::DrawAABB(const Shared<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    if (!mesh) return;

    for (const auto& submesh : mesh->GetSubmeshes())
    {
        const auto& sphere = submesh->GetBoundingSphere();
        DrawAABB(sphere.Center, glm::vec3{sphere.Radius}, transform, color);
    }
}

void DebugRenderer::DrawAABB(const glm::vec3& center, const glm::vec3& halfExtents, const glm::mat4& transform, const glm::vec4& color)
{
    // Top plane
    glm::vec3 lineVertices[4] = {glm::vec3(center + halfExtents),
                                 glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z),
                                 glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z),
                                 glm::vec3(center.x + halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z)};

    const auto DrawLines = [&](const uint32_t lineVertexCount)
    {
        for (uint32_t i{}; i < lineVertexCount; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == lineVertexCount ? 0 : nextLineVertexIndex], color);
        }
    };

    const auto UpdateLineVertices = [&](const uint32_t lineVertexCount)
    {
        for (size_t i{}; i < lineVertexCount; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);
    };

    UpdateLineVertices(4);
    DrawLines(4);

    // Bottom plane
    lineVertices[0] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    lineVertices[1] = glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    lineVertices[2] = glm::vec3(center - halfExtents);
    lineVertices[3] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z);
    UpdateLineVertices(4);
    DrawLines(4);

    // Forward right line
    lineVertices[0] = glm::vec3(center + halfExtents);
    lineVertices[1] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    UpdateLineVertices(2);
    DrawLines(2);

    // Forward left line
    lineVertices[0] = glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z);
    lineVertices[1] = glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
    UpdateLineVertices(2);
    DrawLines(2);

    // Backward right line
    lineVertices[0] = glm::vec3(center.x + halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z);
    lineVertices[1] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z);
    UpdateLineVertices(2);
    DrawLines(2);

    // Backward left line
    lineVertices[0] = glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z);
    lineVertices[1] = glm::vec3(center - halfExtents);
    UpdateLineVertices(2);
    DrawLines(2);
}

void DebugRenderer::DrawSphere(const Shared<Mesh>& mesh, const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                               const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    for (const auto& submesh : mesh->GetSubmeshes())
    {
        const auto& sphere = submesh->GetBoundingSphere();
        DrawSphere(translation, scale, orientation, sphere.Center, sphere.Radius, color);
    }
}

}  // namespace Pathfinder