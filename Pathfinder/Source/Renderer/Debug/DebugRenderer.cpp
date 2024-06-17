#include <PathfinderPCH.h>
#include "DebugRenderer.h"

#include <Renderer/Renderer.h>
#include <Renderer/Pipeline.h>
#include <Renderer/Shader.h>
#include <Renderer/Buffer.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/Mesh/MeshManager.h>
#include <Renderer/Mesh/Mesh.h>
#include <Renderer/Mesh/Submesh.h>

namespace Pathfinder
{
static bool s_bDebugRendererInit = false;

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

    std::ranges::for_each(s_DebugRendererData->LineVertexBase,
                          [](auto& lineVertexBase)
                          {
                              // TODO: Use here CPU-side memory allocator
                              lineVertexBase = new LineVertex[s_DebugRendererData->s_MAX_VERTICES];
                          });

    std::ranges::for_each(s_DebugRendererData->LineVertexBuffer,
                          [](auto& vertexBuffer)
                          {
                              const BufferSpecification vbSpec = {.DebugName  = "LineVertexBuffer",
                                                                  .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED,
                                                                  .UsageFlags = EBufferUsage::BUFFER_USAGE_VERTEX,
                                                                  .Capacity   = s_DebugRendererData->s_MAX_VERTEX_BUFFER_SIZE};
                              vertexBuffer                     = Buffer::Create(vbSpec);
                          });

    for (uint32_t fif{}; fif < s_FRAMES_IN_FLIGHT; ++fif)
    {
        s_DebugRendererData->LineVertexCurrent[fif] = s_DebugRendererData->LineVertexBase[fif];
    }

    ShaderLibrary::WaitUntilShadersLoaded();

    std::ranges::for_each(s_DebugRendererData->DebugSphereSSBO,
                          [](auto& debugSphereSSBO)
                          {
                              const BufferSpecification dsiSSBO = {.DebugName  = "DebugSphereSSBO",
                                                                   .ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                                   .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE |
                                                                                 EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE};
                              debugSphereSSBO                   = Buffer::Create(dsiSSBO);
                          });

    s_DebugRendererData->DebugSphereMesh = MeshManager::GenerateUVSphere(24, 18);

    const auto* gpo = std::get_if<GraphicsPipelineOptions>(
        &PipelineLibrary::Get(Renderer::GetRendererData()->ForwardPlusOpaquePipelineHash)->GetSpecification().PipelineOptions.value());
    PFR_ASSERT(gpo, "GPO is not valid!");

    const GraphicsPipelineOptions sphereGPO = {
        .InputBufferBindings = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}},
        .Formats             = gpo->Formats,
        .CullMode            = ECullMode::CULL_MODE_BACK,
        .FrontFace           = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
        .PrimitiveTopology   = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .LineWidth           = s_DebugRendererData->LineWidth,
        .bBlendEnable        = true,
        .BlendMode           = EBlendMode::BLEND_MODE_ALPHA,
        .PolygonMode         = EPolygonMode::POLYGON_MODE_LINE,
        .bDepthTest          = true,
        .bDepthWrite         = false,
        .DepthCompareOp      = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};

    PipelineSpecification spherePipelineSpec = {.DebugName       = "DebugSphere",
                                                .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(sphereGPO),
                                                .Shader          = ShaderLibrary::Get("Debug/Sphere"),
                                                .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    s_DebugRendererData->SpherePipelineHash  = PipelineLibrary::Push(spherePipelineSpec);

    const GraphicsPipelineOptions lineGPO = {
        .InputBufferBindings = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                 {"inColor", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT}}},
        .Formats             = gpo->Formats,
        .CullMode            = ECullMode::CULL_MODE_BACK,
        .FrontFace           = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
        .PrimitiveTopology   = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_LIST,
        .LineWidth           = s_DebugRendererData->LineWidth,
        .bBlendEnable        = true,
        .BlendMode           = EBlendMode::BLEND_MODE_ALPHA,
        .PolygonMode         = EPolygonMode::POLYGON_MODE_LINE,
        .bDepthTest          = true,
        .bDepthWrite         = false,
        .DepthCompareOp      = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};
    PipelineSpecification linePipelineSpec = {.DebugName       = "DebugLine",
                                              .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(lineGPO),
                                              .Shader          = ShaderLibrary::Get("Debug/Line"),
                                              .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    s_DebugRendererData->LinePipelineHash  = PipelineLibrary::Push(linePipelineSpec);

    PipelineLibrary::Compile();
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
    if (!s_bDebugRendererInit) return;

    auto& rd = Renderer::GetRendererData();
    if (s_DebugRendererData->FrameIndex == rd->FrameIndex) return;

    s_DebugRendererData->FrameIndex = rd->FrameIndex;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
        s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
}

void DebugRenderer::Flush(Shared<CommandBuffer>& renderCommandBuffer)
{
    UPDATE_STATE_ROUTINE;

    PFR_ASSERT(false, "NOT IMPLEMENTED!");
#if 0
    auto& rd = Renderer::GetRendererData();
    renderCommandBuffer->BeginTimestampQuery();

    if (const auto lineDataSize = s_DebugRendererData->LineVertexCount * sizeof(LineVertex); lineDataSize > 0)
    {
        s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]->SetData(
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], lineDataSize);

        renderCommandBuffer->BeginDebugLabel("DebugLines", glm::vec3(.1f, .8f, .3f));
        //    rd->GBuffer->BeginPass(renderCommandBuffer);

        const PushConstantBlock pc = {rd->CameraSSBO[rd->FrameIndex]->GetBDA()};
        constexpr uint64_t offset  = 0;
        renderCommandBuffer->BindVertexBuffers({s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]}, 0, 1, &offset);

        Renderer::BindPipeline(renderCommandBuffer, PipelineLibrary::Get(s_DebugRendererData->LinePipelineHash));
        renderCommandBuffer->BindPushConstants(PipelineLibrary::Get(s_DebugRendererData->LinePipelineHash), 0, 0, sizeof(pc), &pc);
        renderCommandBuffer->Draw(s_DebugRendererData->LineVertexCount);

        //  rd->GBuffer->EndPass(renderCommandBuffer);
        renderCommandBuffer->EndDebugLabel();

        memset(s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], 0, lineDataSize);
        s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
        s_DebugRendererData->LineVertexCount = 0;
    }

    if (!s_DebugRendererData->DebugSpheres.empty())
    {
        s_DebugRendererData->DebugSphereSSBO[s_DebugRendererData->FrameIndex]->SetData(s_DebugRendererData->DebugSpheres.data(),
                                                                                       s_DebugRendererData->DebugSpheres.size() *
                                                                                           sizeof(s_DebugRendererData->DebugSpheres[0]));

        renderCommandBuffer->BeginDebugLabel("DebugSpheres", glm::vec3(.1f, .3f, .8f));
        //       rd->GBuffer->BeginPass(renderCommandBuffer);

        Renderer::BindPipeline(renderCommandBuffer, PipelineLibrary::Get(s_DebugRendererData->SpherePipelineHash));
        PushConstantBlock pc = {rd->CameraSSBO[rd->FrameIndex]->GetBDA()};
        pc.addr0             = s_DebugRendererData->DebugSphereSSBO[s_DebugRendererData->FrameIndex]->GetBDA();
        renderCommandBuffer->BindPushConstants(PipelineLibrary::Get(s_DebugRendererData->SpherePipelineHash), 0, 0, sizeof(pc), &pc);

        constexpr uint64_t offset = 0;
        renderCommandBuffer->BindVertexBuffers({s_DebugRendererData->DebugSphereMesh.VertexBuffer}, 0, 1, &offset);
        renderCommandBuffer->BindIndexBuffer(s_DebugRendererData->DebugSphereMesh.IndexBuffer);

        renderCommandBuffer->DrawIndexed(s_DebugRendererData->DebugSphereMesh.IndexBuffer->GetSpecification().Capacity / sizeof(uint32_t),
                                         s_DebugRendererData->DebugSpheres.size());

        //        rd->GBuffer->EndPass(renderCommandBuffer);
        renderCommandBuffer->EndDebugLabel();
    }
    renderCommandBuffer->EndTimestampQuery();
#endif

    s_DebugRendererData->DebugSpheres.clear();
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