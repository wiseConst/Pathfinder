#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Framebuffer.h"

namespace Pathfinder
{
Renderer::Renderer()  = default;
Renderer::~Renderer() = default;

void Renderer::Init()
{
    s_RendererData = MakeUnique<RendererData>();
    ShaderLibrary::Init();
    PipelineBuilder::Init();

    ShaderLibrary::Load("raytrace");

    PipelineSpecification pathTracingPipelineSpec = {};
    pathTracingPipelineSpec.Shader                = ShaderLibrary::Get("raytrace");
    pathTracingPipelineSpec.DebugName             = "PathTracing";
    pathTracingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
    pathTracingPipelineSpec.TargetFramebuffer     = s_RendererData->CompositeFramebuffer;

    s_RendererData->PathTracingPipeline = Pipeline::Create(pathTracingPipelineSpec);

    s_BindlessRenderer = BindlessRenderer::Create();

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });

    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });

    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D created!");

    Renderer2D::Init();

    ShaderLibrary::DestroyReflectionGarbage();  // should be in the very end
}

void Renderer::Shutdown()
{
    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();
    PipelineBuilder::Shutdown();

    s_BindlessRenderer.reset();
    s_RendererData.reset();
    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D destroyed!");
}

void Renderer::Begin()
{
    s_RendererData->FrameIndex                  = Application::Get().GetWindow()->GetCurrentFrameIndex();
    s_RendererData->CurrentRenderCommandBuffer  = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentComputeCommandBuffer = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex];

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording();
}

void Renderer::Flush()
{
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();

    Application::Get().GetWindow()->CopyToWindow(s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]);
}

void Renderer::BeginScene(const Camera& camera) {}

void Renderer::EndScene() {}

}  // namespace Pathfinder