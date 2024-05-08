#include "PathfinderPCH.h"
#include "DebugRenderer.h"

#include "Renderer/Renderer.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"
#include "Renderer/CommandBuffer.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Mesh/MeshManager.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Mesh/Submesh.h"

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
    s_DebugRendererData = MakeUnique<DebugRendererData>();

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
                              BufferSpecification vbSpec = {EBufferUsage::BUFFER_USAGE_VERTEX};
                              vbSpec.BufferCapacity      = s_DebugRendererData->s_MAX_VERTEX_BUFFER_SIZE;

                              vertexBuffer = Buffer::Create(vbSpec);
                          });

    s_DebugRendererData->FrameIndex = 0;
    for (uint32_t fif{}; fif < s_FRAMES_IN_FLIGHT; ++fif)
    {
        s_DebugRendererData->LineVertexCurrent[fif] = s_DebugRendererData->LineVertexBase[fif];
    }

    {
        BufferSpecification dsiSSBO              = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        s_DebugRendererData->DebugSphereInfoSSBO = Buffer::Create(dsiSSBO);
        s_DebugRendererData->DebugSphereMesh     = MeshManager::GenerateUVSphere(24, 18);

        PipelineSpecification spherePipelineSpec = {"DebugSphere"};
        spherePipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        spherePipelineSpec.Shader                = ShaderLibrary::Get("Debug/Sphere");
        spherePipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions sphereGraphicsPipelineOptions = {};
        sphereGraphicsPipelineOptions.LineWidth               = s_DebugRendererData->LineWidth;
        sphereGraphicsPipelineOptions.bDepthTest              = true;
        sphereGraphicsPipelineOptions.bDepthWrite             = false;
        sphereGraphicsPipelineOptions.DepthCompareOp          = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
        sphereGraphicsPipelineOptions.BlendMode               = EBlendMode::BLEND_MODE_ALPHA;
        sphereGraphicsPipelineOptions.bBlendEnable            = true;
        sphereGraphicsPipelineOptions.PolygonMode             = EPolygonMode::POLYGON_MODE_LINE;
        sphereGraphicsPipelineOptions.PrimitiveTopology       = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        sphereGraphicsPipelineOptions.FrontFace               = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
        sphereGraphicsPipelineOptions.TargetFramebuffer       = Renderer::GetRendererData()->GBuffer;
        sphereGraphicsPipelineOptions.InputBufferBindings = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}};
        spherePipelineSpec.PipelineOptions                = std::make_optional<GraphicsPipelineOptions>(sphereGraphicsPipelineOptions);
        PipelineBuilder::Push(s_DebugRendererData->SpherePipeline, spherePipelineSpec);
    }

    {
        PipelineSpecification linePipelineSpec = {"DebugLine"};
        linePipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        linePipelineSpec.Shader                = ShaderLibrary::Get("Debug/Line");
        linePipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions lineGraphicsPipelineOptions = {};
        lineGraphicsPipelineOptions.LineWidth               = s_DebugRendererData->LineWidth;
        lineGraphicsPipelineOptions.bDepthTest              = true;
        lineGraphicsPipelineOptions.bDepthWrite             = false;
        lineGraphicsPipelineOptions.DepthCompareOp          = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
        lineGraphicsPipelineOptions.BlendMode               = EBlendMode::BLEND_MODE_ALPHA;
        lineGraphicsPipelineOptions.bBlendEnable            = true;
        lineGraphicsPipelineOptions.PolygonMode             = EPolygonMode::POLYGON_MODE_LINE;
        lineGraphicsPipelineOptions.PrimitiveTopology       = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_LIST;
        lineGraphicsPipelineOptions.FrontFace               = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
        lineGraphicsPipelineOptions.TargetFramebuffer       = Renderer::GetRendererData()->GBuffer;
        lineGraphicsPipelineOptions.InputBufferBindings     = {{{"inPosition", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                                                {"inColor", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4}}};
        linePipelineSpec.PipelineOptions                    = std::make_optional<GraphicsPipelineOptions>(lineGraphicsPipelineOptions);
        PipelineBuilder::Push(s_DebugRendererData->LinePipeline, linePipelineSpec);
    }

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
    if (!s_bDebugRendererInit) return;

    auto& rd = Renderer::GetRendererData();
    if (s_DebugRendererData->FrameIndex == rd->FrameIndex) return;

    s_DebugRendererData->FrameIndex = rd->FrameIndex;
    s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
        s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
}

void DebugRenderer::Flush()
{
    UPDATE_STATE_ROUTINE;

    const uint32_t lineDataSize = s_DebugRendererData->LineVertexCount * sizeof(LineVertex);

    auto& rd = Renderer::GetRendererData();
    auto& br = Renderer::GetBindlessRenderer();

    const auto renderCommandBuffer = rd->CurrentRenderCommandBuffer.lock();
    PFR_ASSERT(renderCommandBuffer, "Failed to acquire CurrentRenderCommandBuffer!");

    // TODO: Make immediate submit since it's Flush().
    if (lineDataSize > 0)
    {
        renderCommandBuffer->BeginDebugLabel("Lines");
        rd->GBuffer->BeginPass(renderCommandBuffer);

        s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]->SetData(
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], lineDataSize);

        PushConstantBlock pc = {};
        pc.CameraDataBuffer  = rd->CameraSSBO[rd->FrameIndex]->GetBDA();

        constexpr uint64_t offset = 0;
        renderCommandBuffer->BindVertexBuffers({s_DebugRendererData->LineVertexBuffer[s_DebugRendererData->FrameIndex]}, 0, 1, &offset);

        Renderer::BindPipeline(renderCommandBuffer, s_DebugRendererData->LinePipeline);
        renderCommandBuffer->BindPushConstants(s_DebugRendererData->LinePipeline, 0, 0, sizeof(pc), &pc);
        renderCommandBuffer->Draw(s_DebugRendererData->LineVertexCount);

        rd->GBuffer->EndPass(renderCommandBuffer);
        renderCommandBuffer->EndDebugLabel();

        memset(s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex], 0, lineDataSize);
        s_DebugRendererData->LineVertexCurrent[s_DebugRendererData->FrameIndex] =
            s_DebugRendererData->LineVertexBase[s_DebugRendererData->FrameIndex];
        s_DebugRendererData->LineVertexCount = 0;
    }

    if (!s_DebugRendererData->DebugSphereInfos.empty())
    {
        s_DebugRendererData->DebugSphereInfoSSBO->SetData(s_DebugRendererData->DebugSphereInfos.data(),
                                                          s_DebugRendererData->DebugSphereInfos.size() *
                                                              sizeof(s_DebugRendererData->DebugSphereInfos[0]));

        renderCommandBuffer->BeginDebugLabel("Lines");
        rd->GBuffer->BeginPass(renderCommandBuffer);

        constexpr uint64_t offset = 0;
        renderCommandBuffer->BindVertexBuffers({s_DebugRendererData->DebugSphereMesh.VertexBuffer}, 0, 1, &offset);
        renderCommandBuffer->BindIndexBuffer(s_DebugRendererData->DebugSphereMesh.IndexBuffer);

        Renderer::BindPipeline(renderCommandBuffer, s_DebugRendererData->SpherePipeline);

        PushConstantBlock pc = {};
        pc.CameraDataBuffer  = rd->CameraSSBO[rd->FrameIndex]->GetBDA();
        for (uint32_t i = 0; i < s_DebugRendererData->DebugSphereInfos.size(); ++i)
        {
            pc.LightDataBuffer   = s_DebugRendererData->DebugSphereInfoSSBO->GetBDA();
            pc.StorageImageIndex = i;
            renderCommandBuffer->BindPushConstants(s_DebugRendererData->SpherePipeline, 0, 0, sizeof(pc), &pc);
            renderCommandBuffer->DrawIndexed(s_DebugRendererData->DebugSphereMesh.IndexBuffer->GetSpecification().BufferCapacity /
                                             sizeof(uint32_t));
        }

        rd->GBuffer->EndPass(renderCommandBuffer);
        renderCommandBuffer->EndDebugLabel();
    }

    s_DebugRendererData->DebugSphereInfos.clear();
}

void DebugRenderer::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

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

void DebugRenderer::DrawSphere(const Shared<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color)
{
    UPDATE_STATE_ROUTINE;

    for (const auto& submesh : mesh->GetSubmeshes())
    {
        const auto& sphere = submesh->GetBoundingSphere();
        DrawSphere(sphere.Center, sphere.Radius, transform, color);
    }
}

void DebugRenderer::DrawSphere(const glm::vec3& center, const float radius, const glm::mat4& transform, const glm::vec4& color)
{
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform, scale, rotation, translation, skew, perspective);
    s_DebugRendererData->DebugSphereInfos.emplace_back(translation, glm::vec4{rotation.x, rotation.y, rotation.z, rotation.w}, scale,
                                                       center, radius, color);
}

}  // namespace Pathfinder