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
#include "Mesh.h"

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

    ShaderLibrary::Load(std::vector<std::string>{"pathtrace", "GBuffer", "Forward"});

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });

    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });

    std::ranges::for_each(s_RendererData->CompositeFramebuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {};
                              framebufferSpec.Name                     = "Composite";

                              framebufferSpec.Attachments = {
                                  {EImageFormat::FORMAT_RGBA8_UNORM /*EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32*/, ELoadOp::CLEAR,
                                   EStoreOp::STORE, glm::vec4(0.15f), true}};
                              framebuffer = Framebuffer::Create(framebufferSpec);
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

    // Fallback to default graphics pipeline
    if (s_RendererSettings.bMeshShadingSupport)
    {
        ShaderLibrary::Load("ms");

        PipelineSpecification testMeshShadingPipelineSpec = {};
        testMeshShadingPipelineSpec.Shader                = ShaderLibrary::Get("ms");
        testMeshShadingPipelineSpec.DebugName             = "TestMeshShading";
        testMeshShadingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        testMeshShadingPipelineSpec.bMeshShading          = true;
        testMeshShadingPipelineSpec.TargetFramebuffer     = s_RendererData->CompositeFramebuffer;
        PipelineBuilder::Push(s_RendererData->TestMeshShadingPipeline, testMeshShadingPipelineSpec);
    }

    PipelineSpecification forwardPipelineSpec  = {};
    forwardPipelineSpec.Shader                 = ShaderLibrary::Get("Forward");
    forwardPipelineSpec.DebugName              = "ForwardRendering";
    forwardPipelineSpec.PipelineType           = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    forwardPipelineSpec.bBindlessCompatible    = true;
    forwardPipelineSpec.bSeparateVertexBuffers = true;
    //  forwardPipelineSpec.CullMode               = ECullMode::CULL_MODE_BACK;
    forwardPipelineSpec.TargetFramebuffer = s_RendererData->CompositeFramebuffer;
    PipelineBuilder::Push(s_RendererData->ForwardRenderingPipeline, forwardPipelineSpec);

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
    ShaderLibrary::DestroyGarbageIfNeeded();
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

    s_RendererStats                           = {};
    Renderer2D::GetRendererData()->FrameIndex = s_RendererData->FrameIndex;
    Renderer2D::Begin();
}

void Renderer::Flush()
{
    GeometryPass();

    Renderer2D::Flush();

    if (s_RendererSettings.bMeshShadingSupport)
    {
        /*     MESH SHADING TESTS       */
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
            s_RendererData->TestMeshShadingPipeline->GetSpecification().DebugName, glm::vec3(0.1f, 0.7f, 0.1f));
        s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->BeginPass(
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->TestMeshShadingPipeline);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(1, 1, 1);

        s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->EndPass(
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
        /*     MESH SHADING TESTS       */
    }

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    // Application::Get().GetWindow()->CopyToWindow(s_RendererData->PathtracedImage[s_RendererData->FrameIndex]);
    Application::Get().GetWindow()->CopyToWindow(
        s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetAttachments()[0].Attachment);
}

void Renderer::BeginScene(const Camera& camera)
{
    s_RendererData->CameraData = {camera.GetProjection(), camera.GetInverseView(), camera.GetPosition()};

    s_RendererData->CameraUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraData, sizeof(s_RendererData->CameraData));

    s_BindlessRenderer->UpdateCameraData(s_RendererData->CameraUB[s_RendererData->FrameIndex]);

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
        s_RendererData->PathtracingPipeline->GetSpecification().DebugName, glm::vec3(0.8f, 0.8f, 0.1f));
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

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::EndScene() {}

void Renderer::GeometryPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.9f, 0.5f, 0.2f));
    s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->BeginPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->ForwardRenderingPipeline);

    auto renderMeshFunc = [](const Shared<Mesh>& mesh)
    {
        for (uint32_t i = 0; i < mesh->GetIndexBuffers().size(); ++i)
        {
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(mesh->GetIndexBuffers()[0]);

            const uint64_t offsets[2] = {0, 0};
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{mesh->GetVertexPositionBuffers()[i], mesh->GetVertexAttributeBuffers()[i]}, 0, 2, offsets);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                mesh->GetIndexBuffers()[0]->GetSpecification().BufferCapacity / sizeof(uint32_t));

            s_RendererStats.TriangleCount += mesh->GetIndexBuffers()[0]->GetSpecification().BufferCapacity / sizeof(uint32_t);
        }
    };

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));
    for (auto& opaque : s_RendererData->OpaqueObjects)
    {
        renderMeshFunc(opaque);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    for (auto& transparent : s_RendererData->TransparentObjects)
    {
        renderMeshFunc(transparent);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->EndPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::SubmitMesh(const Shared<Mesh>& mesh)
{
    if (mesh->IsOpaque()[0])
        s_RendererData->OpaqueObjects.emplace_back(mesh);
    else
        s_RendererData->TransparentObjects.emplace_back(mesh);
}

}  // namespace Pathfinder