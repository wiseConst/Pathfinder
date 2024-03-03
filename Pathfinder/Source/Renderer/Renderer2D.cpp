#include "PathfinderPCH.h"
#include "Renderer2D.h"

#include "Renderer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "Framebuffer.h"

namespace Pathfinder
{

Renderer2D::Renderer2D()  = default;
Renderer2D::~Renderer2D() = default;

void Renderer2D::Init()
{
    s_RendererData2D = MakeUnique<RendererData2D>();
    memset(&s_Renderer2DStats, 0, sizeof(s_Renderer2DStats));

    ShaderLibrary::Load("Quad2D");

    std::ranges::for_each(s_RendererData2D->QuadVertexBase,
                          [](auto& quadVertexBase)
                          {
                              // TODO: Use here CPU-side memory allocator
                              quadVertexBase = new QuadVertex[s_RendererData2D->s_MAX_VERTICES];
                          });

    std::ranges::for_each(s_RendererData2D->QuadVertexBuffer,
                          [](auto& vertexBuffer)
                          {
                              BufferSpecification vbSpec = {EBufferUsage::BUFFER_USAGE_VERTEX};
                              vbSpec.BufferCapacity      = s_RendererData2D->s_MAX_VERTEX_BUFFER_SIZE;

                              vertexBuffer = Buffer::Create(vbSpec);
                          });

    {
        // TODO: Use here memory allocator
        uint32_t* indices = new uint32_t[s_RendererData2D->s_MAX_INDICES];

        // Counter-Clockwise order
        uint32_t offset = 0;
        for (uint32_t i = 0; i < s_RendererData2D->s_MAX_INDICES; i += 6)
        {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;

            indices[i + 3] = offset + 2;
            indices[i + 4] = offset + 3;
            indices[i + 5] = offset + 0;

            offset += 4;
        }

        BufferSpecification ibSpec = {EBufferUsage::BUFFER_USAGE_INDEX};
        ibSpec.Data                = indices;
        ibSpec.DataSize            = sizeof(uint32_t) * s_RendererData2D->s_MAX_INDICES;

        s_RendererData2D->QuadIndexBuffer = Buffer::Create(ibSpec);

        delete[] indices;
    }

    PipelineSpecification quadPipelineSpec = {"Quad2D", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    quadPipelineSpec.Shader                = ShaderLibrary::Get("Quad2D");
    quadPipelineSpec.bBindlessCompatible   = true;
    quadPipelineSpec.FrontFace             = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    quadPipelineSpec.TargetFramebuffer     = Renderer::GetRendererData()->GBuffer;
    // quadPipelineSpec.bDepthTest            = true;
    // quadPipelineSpec.bDepthWrite           = true;  // Do I rly need this? since im using depth pre pass of my static meshes
    // quadPipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
    PipelineBuilder::Push(s_RendererData2D->QuadPipeline, quadPipelineSpec);

    PipelineBuilder::Build();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D created!");
}

void Renderer2D::Shutdown()
{
    std::ranges::for_each(s_RendererData2D->QuadVertexBase,
                          [](QuadVertex* quadVertexBase)
                          {
                              // TODO: Use here memory allocator
                              delete[] quadVertexBase;
                          });

    s_RendererData2D.reset();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D destroyed!");
}

void Renderer2D::Begin()
{
    s_Renderer2DStats = {};

    auto& rd = Renderer::GetRendererData();

    s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex] = s_RendererData2D->QuadVertexBase[s_RendererData2D->FrameIndex];
    s_RendererData2D->Sprites.clear();
}

void Renderer2D::FlushBatch()
{
    const uint32_t dataSize = s_Renderer2DStats.QuadCount * sizeof(QuadVertex) * 4;
    if (dataSize == 0) return;

    auto renderCommandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS);
    renderCommandBuffer->BeginRecording(true);

    auto& rd = Renderer::GetRendererData();
    auto& br = Renderer::GetBindlessRenderer();
    br->Bind(renderCommandBuffer);

    rd->GBuffer->BeginPass(renderCommandBuffer);

    s_RendererData2D->QuadVertexBuffer[s_RendererData2D->FrameIndex]->SetData(
        s_RendererData2D->QuadVertexBase[s_RendererData2D->FrameIndex], dataSize);

    PushConstantBlock pc = {};
    renderCommandBuffer->BindPipeline(s_RendererData2D->QuadPipeline);
    renderCommandBuffer->BindPushConstants(s_RendererData2D->QuadPipeline, 0, 0, sizeof(pc), &pc);

    constexpr uint64_t offset = 0;
    renderCommandBuffer->BindVertexBuffers({s_RendererData2D->QuadVertexBuffer[s_RendererData2D->FrameIndex]}, 0, 1, &offset);
    renderCommandBuffer->BindIndexBuffer(s_RendererData2D->QuadIndexBuffer);

    renderCommandBuffer->DrawIndexed(s_Renderer2DStats.QuadCount * 6);

    rd->GBuffer->EndPass(renderCommandBuffer);

    renderCommandBuffer->EndRecording();
    renderCommandBuffer->Submit();

    memset(s_RendererData2D->QuadVertexBase[s_RendererData2D->FrameIndex], 0, dataSize);
    s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex] = s_RendererData2D->QuadVertexBase[s_RendererData2D->FrameIndex];
    ++s_Renderer2DStats.BatchCount;
    s_Renderer2DStats.QuadCount = 0;
}

void Renderer2D::Flush()
{
    FlushBatch();
}

void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, const uint32_t layer)
{
    if (s_Renderer2DStats.QuadCount >= s_RendererData2D->s_MAX_QUADS) Flush();

    for (uint32_t i = 0; i < 4; ++i)
    {
        s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex]->Position =
            transform * glm::vec4(s_RendererData2D->QuadVertices[i], 1.0f);
        s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex]->Color  = color;
        s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex]->Normal = s_RendererData2D->QuadNormals[i];
        s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex]->UV     = s_RendererData2D->QuadUVs[i];

        ++s_RendererData2D->QuadVertexCurrent[s_RendererData2D->FrameIndex];
    }

    ++s_Renderer2DStats.QuadCount;
    s_Renderer2DStats.TriangleCount += 2;
}

}  // namespace Pathfinder