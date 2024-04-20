#include "PathfinderPCH.h"
#include "DebugRenderer.h"

#include "Renderer/Renderer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Mesh/Submesh.h"

namespace Pathfinder
{
static bool s_bDebugRendererInit = false;

void DebugRenderer::Init()
{
    PFR_ASSERT(!s_bDebugRendererInit, "DebugRenderer already initialized!");
    s_DebugRendererData = MakeUnique<DebugRendererData>();

    ShaderLibrary::Load("Line");

    std::ranges::for_each(s_DebugRendererData->LineVertexBase,
                          [](auto& lineVertexBase)
                          {
                              // TODO: Use here CPU-side memory allocator
                              lineVertexBase = new LineVertex[s_DebugRendererData->s_MAX_VERTICES];
                          });

    std::ranges::for_each(s_DebugRendererData->LineVertexBuffer,
                          [](auto& vertexBuffer)
                          {
                              BufferSpecification vbSpec = {EBufferUsage::BUFFER_USAGE_VERTEX};
                              vbSpec.BufferCapacity      = s_DebugRendererData->s_MAX_VERTEX_BUFFER_SIZE;

                              vertexBuffer = Buffer::Create(vbSpec);
                          });

    s_DebugRendererData->FrameIndex = 0;
    for (uint32_t fif{}; fif < s_FRAMES_IN_FLIGHT; ++fif)
    {
        s_DebugRendererData->LineVertexCurrent[fif] = s_DebugRendererData->LineVertexBase[fif];
    }

    PipelineSpecification linePipelineSpec = {"Line", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    linePipelineSpec.Shader                = ShaderLibrary::Get("Line");
    linePipelineSpec.LineWidth             = s_DebugRendererData->LineWidth;
    linePipelineSpec.bDepthTest            = true;
    linePipelineSpec.bDepthWrite           = false;
    linePipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    linePipelineSpec.BlendMode             = EBlendMode::BLEND_MODE_ALPHA;
    linePipelineSpec.bBlendEnable          = true;
    linePipelineSpec.PolygonMode           = EPolygonMode::POLYGON_MODE_LINE;
    linePipelineSpec.PrimitiveTopology     = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_LIST;
    linePipelineSpec.bBindlessCompatible   = true;
    linePipelineSpec.FrontFace             = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    linePipelineSpec.TargetFramebuffer     = Renderer::GetRendererData()->GBuffer;
    linePipelineSpec.InputBufferBindings   = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                               {"inColor", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4}}};
    PipelineBuilder::Push(s_DebugRendererData->LinePipeline, linePipelineSpec);

    PipelineBuilder::Build();

    LOG_TAG_TRACE(DEBUG_RENDERER, "DebugRenderer created!");
    s_bDebugRendererInit = true;
}

void DebugRenderer::Shutdown()
{
    if (s_bDebugRendererInit)
    {
        s_DebugRendererData.reset();
        s_bDebugRendererInit = false;
        LOG_TAG_TRACE(DEBUG_RENDERER, "DebugRenderer destroyed!");
    }
}

void DebugRenderer::UpdateState()
{
    auto& rd = Renderer::GetRendererData();
    if (s_DebugRendererData->FrameIndex == rd->FrameIndex) return;

    s_DebugRendererData->FrameIndex = rd->FrameIndex;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
        s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
}

void DebugRenderer::Flush()
{
    if (!s_bDebugRendererInit) return;

    UpdateState();

    const uint32_t dataSize = s_DebugRendererData->LineVertexCount * sizeof(LineVertex);
    if (dataSize == 0) return;

    auto& rd = Renderer::GetRendererData();
    auto& br = Renderer::GetBindlessRenderer();

    // TODO: Make immediate submit since it's Flush().
    if (const auto renderCommandBuffer = rd->CurrentRenderCommandBuffer.lock())
    {
        rd->GBuffer->BeginPass(renderCommandBuffer);

        s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]->SetData(
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], dataSize);

        Renderer::BindPipeline(renderCommandBuffer, s_DebugRendererData->LinePipeline);

        constexpr uint64_t offset = 0;
        renderCommandBuffer->BindVertexBuffers({s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]}, 0, 1, &offset);
        renderCommandBuffer->Draw(s_DebugRendererData->LineVertexCount);

        rd->GBuffer->EndPass(renderCommandBuffer);

        memset(s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], 0, dataSize);
        s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
        s_DebugRendererData->LineVertexCount = 0;
    }
}

void DebugRenderer::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
{
    if (!s_bDebugRendererInit) return;

    UpdateState();

    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Position = p0;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Color    = color;
    ++s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex];

    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Position = p1;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex]->Color    = color;
    ++s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex];

    s_DebugRendererData->LineVertexCount += 2;
}

void DebugRenderer::DrawRect(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color)
{
    if (!s_bDebugRendererInit) return;

    UpdateState();

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
    if (!s_bDebugRendererInit) return;

    UpdateState();

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
    if (!s_bDebugRendererInit) return;

    UpdateState();

    if (!mesh) return;

    for (const auto& submesh : mesh->GetSubmeshes())
    {
        const auto& sphere = submesh->GetBoundingSphere();

        const glm::vec3 center = sphere.Center;
        glm::vec3 halfExtents  = glm::vec3(sphere.Radius);

        // Top plane
        glm::vec3 lineVertices[4] = {glm::vec3(center + halfExtents),
                                     glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z),
                                     glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z),
                                     glm::vec3(center.x + halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z)};
        for (size_t i{}; i < 4; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 4; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 4 ? 0 : nextLineVertexIndex], color);
        }

        // Bottom plane
        lineVertices[0] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
        lineVertices[1] = glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
        lineVertices[2] = glm::vec3(center - halfExtents);
        lineVertices[3] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z);
        for (size_t i{}; i < 4; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 4; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 4 ? 0 : nextLineVertexIndex], color);
        }

        // Forward right line
        lineVertices[0] = glm::vec3(center + halfExtents);
        lineVertices[1] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
        for (size_t i{}; i < 2; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 2; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 2 ? 0 : nextLineVertexIndex], color);
        }

        // Forward left line
        lineVertices[0] = glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z);
        lineVertices[1] = glm::vec3(center.x - halfExtents.x, center.y - halfExtents.y, center.z + halfExtents.z);
        for (size_t i{}; i < 2; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 2; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 2 ? 0 : nextLineVertexIndex], color);
        }

        // Backward right line
        lineVertices[0] = glm::vec3(center.x + halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z);
        lineVertices[1] = glm::vec3(center.x + halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z);
        for (size_t i{}; i < 2; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 2; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 2 ? 0 : nextLineVertexIndex], color);
        }

        // Backward left line
        lineVertices[0] = glm::vec3(center.x - halfExtents.x, center.y + halfExtents.y, center.z - halfExtents.z);
        lineVertices[1] = glm::vec3(center - halfExtents);
        for (size_t i{}; i < 2; ++i)
            lineVertices[i] = transform * glm::vec4(lineVertices[i], 1.0f);

        for (uint32_t i{}; i < 2; ++i)
        {
            const uint32_t nextLineVertexIndex = i + 1;
            DrawLine(lineVertices[i], lineVertices[nextLineVertexIndex == 2 ? 0 : nextLineVertexIndex], color);
        }
    }
}

}  // namespace Pathfinder