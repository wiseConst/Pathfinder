#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "CommandBuffer.h"
#include "Shader.h"

namespace Pathfinder
{

void Renderer::Init()
{
    s_RendererData = MakeUnique<RendererData>();
    ShaderLibrary::Init();

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });

    LOG_TAG(RENDERER_3D, "Renderer3D created!");

    Renderer2D::Init();
}

void Renderer::Shutdown()
{
    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();

    s_RendererData.reset();
    LOG_TAG(RENDERER_3D, "Renderer3D destroyed!");
}

void Renderer::Begin()
{
    s_RendererData->FrameIndex                 = Application::Get().GetWindow()->GetCurrentFrameIndex();
    s_RendererData->CurrentRenderCommandBuffer = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex];

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording();
}

void Renderer::Flush()
{
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->CurrentRenderCommandBuffer.reset();
}

void Renderer::BeginScene(const Camera& camera) {}

void Renderer::EndScene() {}

}  // namespace Pathfinder