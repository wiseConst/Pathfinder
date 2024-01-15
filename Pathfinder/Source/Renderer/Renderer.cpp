#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Camera/Camera.h"

namespace Pathfinder
{

Renderer::Renderer()  = default;
Renderer::~Renderer() = default;

void Renderer::Init()
{
    s_RendererData = MakeUnique<RendererData>();
    ShaderLibrary::Init();
    PipelineBuilder::Init();
    s_BindlessRenderer = BindlessRenderer::Create();

    ShaderLibrary::Load(std::vector<std::string>{"pathtrace", "GBuffer"});

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });

    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });

    std::ranges::for_each(s_RendererData->CompositeFramebuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {};
                              framebufferSpec.Name                     = "Composite";

                              framebufferSpec.Attachments = {{EImageFormat::FORMAT_RGBA8_UNORM /*FORMAT_A2R10G10B10_UNORM_PACK32*/,
                                                              ELoadOp::CLEAR, EStoreOp::STORE, glm::vec4(0.7f), true}};
                              framebuffer                 = Framebuffer::Create(framebufferSpec);
                          });

    std::ranges::for_each(s_RendererData->GBuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {};
                              framebufferSpec.Name                     = "GBuffer";

                              FramebufferAttachmentSpecification attachmentSpec = {};
                              attachmentSpec.LoadOp                             = ELoadOp::CLEAR;
                              attachmentSpec.StoreOp                            = EStoreOp::STORE;

                              // positions
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA16F;
                              framebufferSpec.Attachments.push_back(attachmentSpec);

                              // normals
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA16F;
                              framebufferSpec.Attachments.push_back(attachmentSpec);

                              // albedo
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA8_UNORM;
                              framebufferSpec.Attachments.push_back(attachmentSpec);

                              // depth
                              attachmentSpec.Format = EImageFormat::FORMAT_D32F;
                              framebufferSpec.Attachments.push_back(attachmentSpec);

                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height)
        { std::ranges::for_each(s_RendererData->GBuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); }); });

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height)
        { std::ranges::for_each(s_RendererData->CompositeFramebuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); }); });

    std::ranges::for_each(s_RendererData->PathtracedImage,
                          [&](auto& image)
                          {
                              ImageSpecification imageSpec = {};
                              imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
                              imageSpec.UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;

                              image = Image::Create(imageSpec);
                          });
    s_BindlessRenderer->LoadImage(s_RendererData->PathtracedImage);

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height)
        {
            std::ranges::for_each(s_RendererData->PathtracedImage, [&](auto& image) { image->Resize(width, height); });
            s_BindlessRenderer->LoadImage(s_RendererData->PathtracedImage);
        });

    PipelineSpecification pathTracingPipelineSpec = {};
    pathTracingPipelineSpec.Shader                = ShaderLibrary::Get("pathtrace");
    pathTracingPipelineSpec.DebugName             = "Pathtracing";
    pathTracingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
    pathTracingPipelineSpec.bBindlessCompatible   = true;
    PipelineBuilder::Push(s_RendererData->PathtracingPipeline, pathTracingPipelineSpec);

    std::ranges::for_each(s_RendererData->CameraUB,
                          [](Shared<Buffer>& cameraUB)
                          {
                              BufferSpecification vbSpec = {};
                              vbSpec.BufferCapacity      = sizeof(CameraData);
                              vbSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_UNIFORM;
                              vbSpec.bMapPersistent      = true;
                              vbSpec.Data                = &s_RendererData->CameraData;
                              vbSpec.DataSize            = sizeof(s_RendererData->CameraData);

                              cameraUB = Buffer::Create(vbSpec);
                          });

    Renderer2D::Init();

    PipelineBuilder::Build();
    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D created!");
}

void Renderer::Shutdown()
{
    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();
    PipelineBuilder::Shutdown();

    s_RendererData.reset();
    s_BindlessRenderer.reset();
    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D destroyed!");
}

void Renderer::Begin()
{
    s_RendererData->FrameIndex                  = Application::Get().GetWindow()->GetCurrentFrameIndex();
    s_RendererData->CurrentRenderCommandBuffer  = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentComputeCommandBuffer = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex];

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);

    Renderer2D::GetRendererData()->FrameIndex = s_RendererData->FrameIndex;
    Renderer2D::Begin();
}

void Renderer::Flush()
{
    GeometryPass();

    Renderer2D::Flush();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();

    // Application::Get().GetWindow()->CopyToWindow(s_RendererData->PathtracedImage[s_RendererData->FrameIndex]);
    Application::Get().GetWindow()->CopyToWindow(
        s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetAttachments()[0].Attachment);
}

void Renderer::BeginScene(const Camera& camera)
{
    s_RendererData->CameraData.Projection  = camera.GetProjection();
    s_RendererData->CameraData.InverseView = camera.GetInverseView();

    s_RendererData->CameraUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraData, sizeof(s_RendererData->CameraData));

    s_BindlessRenderer->UpdateCameraData(s_RendererData->CameraUB[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex], false);

    struct PC
    {
        glm::mat4 Transform = glm::mat4(1.0f);
        uint32_t img        = 0;
        uint32_t tex        = 0;
    } pc;

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->PathtracingPipeline);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        s_RendererData->PathtracingPipeline,
        EShaderStage::SHADER_STAGE_COMPUTE | EShaderStage::SHADER_STAGE_FRAGMENT | EShaderStage::SHADER_STAGE_VERTEX, 0, sizeof(PC), &pc);

    static constexpr uint32_t workgroup_width  = 16;
    static constexpr uint32_t workgroup_height = 8;

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        (s_RendererData->PathtracedImage[s_RendererData->FrameIndex]->GetSpecification().Width + workgroup_width - 1) / workgroup_width,
        (s_RendererData->PathtracedImage[s_RendererData->FrameIndex]->GetSpecification().Height + workgroup_height - 1) / workgroup_height,
        1);
}

void Renderer::EndScene() {}

void Renderer::GeometryPass()
{
  //  s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    for (auto& opaque : s_RendererData->OpaqueObjects)
    {
    }

    for (auto& transparent : s_RendererData->TransparentObjects)
    {
    }

    //s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->EndPass(    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
}

}  // namespace Pathfinder