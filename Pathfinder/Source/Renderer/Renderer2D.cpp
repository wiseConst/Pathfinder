#include "PathfinderPCH.h"
#include "Renderer2D.h"

#include "Framebuffer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"

namespace Pathfinder
{

Renderer2D::Renderer2D()  = default;
Renderer2D::~Renderer2D() = default;

void Renderer2D::Init()
{
    s_RendererData2D = MakeUnique<RendererData2D>();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D created!");

    ShaderLibrary::Load("Quad2D");

    std::ranges::for_each(s_RendererData2D->QuadVertexBuffer,
                          [](Shared<Buffer>& vertexBuffer)
                          {
                              BufferSpecification vbSpec = {};
                              vbSpec.BufferCapacity      = 8192;
                              vbSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_VERTEX;

                              vertexBuffer = Buffer::Create(vbSpec);
                          });

    PipelineSpecification quadPipelineSpec = {"Quad2D"};
    quadPipelineSpec.Shader                = ShaderLibrary::Get("Quad2D");
    quadPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;

    PipelineBuilder::Push(s_RendererData2D->QuadPipeline, quadPipelineSpec);

    // PipelineBuilder::Build();
}

void Renderer2D::Shutdown()
{
    s_RendererData2D.reset();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D destroyed!");
}

void Renderer2D::Begin() {}

void Renderer2D::Flush() {}

}  // namespace Pathfinder