#include "PathfinderPCH.h"
#include "Renderer2D.h"

#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"

namespace Pathfinder
{

void Renderer2D::Init()
{
    s_RendererData2D = MakeUnique<RendererData2D>();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D created!");

    ShaderLibrary::Load("Quad2D");

    // PipelineSpecification quadPipelineSpec = {"Quad2D"};
    // quadPipelineSpec.Shader                = ShaderLibrary::Get("Quad2D");
    // quadPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    //
    // s_RendererData2D->QuadPipeline = Pipeline::Create(quadPipelineSpec);
}

void Renderer2D::Shutdown()
{
    s_RendererData2D.reset();
    LOG_TAG_TRACE(RENDERER_2D, "Renderer2D destroyed!");
}

}  // namespace Pathfinder