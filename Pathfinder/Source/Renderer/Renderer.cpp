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
    PFR_ASSERT(s_RendererSettings.bBDASupport, "BDA support required!");

    s_RendererData = MakeUnique<RendererData>();
    ShaderLibrary::Init();
    PipelineBuilder::Init();
    s_BindlessRenderer = BindlessRenderer::Create();

    ShaderLibrary::Load(std::vector<std::string>{"pathtrace", "GBuffer", "Forward", "DepthPrePass"});

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });

    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [&](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });

    std::ranges::for_each(s_RendererData->CompositeFramebuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"Composite"};

                              FramebufferAttachmentSpecification compositeAttachmentSpec = {};
                              compositeAttachmentSpec.Format =
                                  EImageFormat::FORMAT_RGBA8_UNORM;  // /*EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32*/
                              compositeAttachmentSpec.ClearColor = glm::vec4(0.15f);
                              compositeAttachmentSpec.LoadOp     = ELoadOp::CLEAR;
                              compositeAttachmentSpec.StoreOp    = EStoreOp::STORE;
                              compositeAttachmentSpec.bCopyable  = true;

                              framebufferSpec.AttachmentsToCreate.emplace_back(compositeAttachmentSpec);
                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    std::ranges::for_each(s_RendererData->GBuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"GBuffer"};

                              FramebufferAttachmentSpecification attachmentSpec = {};
                              attachmentSpec.LoadOp                             = ELoadOp::CLEAR;
                              attachmentSpec.StoreOp                            = EStoreOp::STORE;

                              // positions
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA16F;
                              framebufferSpec.AttachmentsToCreate.push_back(attachmentSpec);

                              // normals
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA16F;
                              framebufferSpec.AttachmentsToCreate.push_back(attachmentSpec);

                              // albedo
                              attachmentSpec.Format = EImageFormat::FORMAT_RGBA8_UNORM;
                              framebufferSpec.AttachmentsToCreate.push_back(attachmentSpec);

                              // depth
                              attachmentSpec.Format = EImageFormat::FORMAT_D32F;
                              framebufferSpec.AttachmentsToCreate.push_back(attachmentSpec);

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

    PipelineSpecification forwardPipelineSpec  = {"Forward+", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    forwardPipelineSpec.Shader                 = ShaderLibrary::Get("Forward");
    forwardPipelineSpec.bBindlessCompatible    = true;
    forwardPipelineSpec.bSeparateVertexBuffers = true;
    forwardPipelineSpec.CullMode               = ECullMode::CULL_MODE_BACK;
    forwardPipelineSpec.TargetFramebuffer      = s_RendererData->CompositeFramebuffer;
    forwardPipelineSpec.bMeshShading           = s_RendererSettings.bMeshShadingSupport;
    PipelineBuilder::Push(s_RendererData->ForwardRenderingPipeline, forwardPipelineSpec);

    std::ranges::for_each(s_RendererData->DepthPrePassFramebuffer,
                          [&](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"DepthPrePass"};

                              FramebufferAttachmentSpecification depthAttachmentSpec = {};
                              depthAttachmentSpec.Format                             = EImageFormat::FORMAT_D32F;
                              depthAttachmentSpec.ClearColor                         = glm::vec4(1.0f);
                              depthAttachmentSpec.LoadOp                             = ELoadOp::CLEAR;
                              depthAttachmentSpec.StoreOp                            = EStoreOp::STORE;
                              depthAttachmentSpec.bCopyable                          = true;

                              framebufferSpec.AttachmentsToCreate.emplace_back(depthAttachmentSpec);
                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height) {
            std::ranges::for_each(s_RendererData->DepthPrePassFramebuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); });
        });

    PipelineSpecification depthPrePassPipelineSpec = {"DepthPrePass", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    depthPrePassPipelineSpec.Shader                = ShaderLibrary::Get("DepthPrePass");
    depthPrePassPipelineSpec.bBindlessCompatible   = true;
    depthPrePassPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    depthPrePassPipelineSpec.TargetFramebuffer     = s_RendererData->DepthPrePassFramebuffer;
    depthPrePassPipelineSpec.bMeshShading          = s_RendererSettings.bMeshShadingSupport;
    depthPrePassPipelineSpec.bDepthTest            = true;
    depthPrePassPipelineSpec.bDepthWrite           = true;
    depthPrePassPipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
    PipelineBuilder::Push(s_RendererData->DepthPrePassPipeline, depthPrePassPipelineSpec);

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

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
}

void Renderer::Flush()
{
    DepthPrePass();
    GeometryPass();

    Renderer2D::Flush();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit();

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    //  Application::Get().GetWindow()->CopyToWindow(s_RendererData->PathtracedImage[s_RendererData->FrameIndex]);
    Application::Get().GetWindow()->CopyToWindow(
        s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetAttachments()[0].Attachment);
}

void Renderer::BeginScene(const Camera& camera)
{
    s_RendererData->CameraData = {camera.GetProjection(), camera.GetInverseView(), camera.GetPosition()};

    s_RendererData->CameraUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraData, sizeof(s_RendererData->CameraData));

    s_BindlessRenderer->UpdateCameraData(s_RendererData->CameraUB[s_RendererData->FrameIndex]);

    /* PATHTRACING TESTS from vk_mini_path_tracer */
    if (s_RendererSettings.bRTXSupport)
    {
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
            s_RendererData->PathtracingPipeline->GetSpecification().DebugName, glm::vec3(0.8f, 0.8f, 0.1f));

        PCBlock pc           = {};
        pc.StorageImageIndex = s_RendererData->PathtracedImage[s_RendererData->FrameIndex]->GetBindlessIndex();
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->PathtracingPipeline);
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->PathtracingPipeline, 0, 0,
                                                                                            sizeof(pc), &pc);

        static constexpr uint32_t workgroup_width  = 16;
        static constexpr uint32_t workgroup_height = 8;

        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            (s_RendererData->PathtracedImage[s_RendererData->FrameIndex]->GetSpecification().Width + workgroup_width - 1) / workgroup_width,
            (s_RendererData->PathtracedImage[s_RendererData->FrameIndex]->GetSpecification().Height + workgroup_height - 1) /
                workgroup_height,
            1);

        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
    }
    /* PATHTRACING TESTS from vk_mini_path_tracer */
}

void Renderer::EndScene() {}

void Renderer::DepthPrePass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.2f, 0.2f, 0.2f));

    s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->BeginPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->DepthPrePassPipeline);

    auto renderMeshFunc = [&](const Shared<Submesh>& submesh)
    {
        PCBlock pc = {};
        //  pc.Transform               = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
        pc.VertexPosBufferIndex = submesh->GetVertexPositionBuffer()->GetBindlessIndex();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
            s_RendererData->DepthPrePassPipeline, s_RendererData->DepthPrePassPipeline->GetSpecification().Shader);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex = submesh->GetMeshletBuffer()->GetBindlessIndex();
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            const uint32_t meshletCount = submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            const uint64_t offset = 0;
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{submesh->GetVertexPositionBuffer()}, 0, 1, &offset);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(submesh->GetIndexBuffer());
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t));
        }
        s_RendererStats.TriangleCount += submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
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

    s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->EndPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::GeometryPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.9f, 0.5f, 0.2f));
    s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->BeginPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->ForwardRenderingPipeline);

    auto renderMeshFunc = [&](const Shared<Submesh>& submesh)
    {
        PCBlock pc = {};
        //  pc.Transform               = glm::scale(glm::mat4(1.0f), glm::vec3(0.05f));
        pc.VertexPosBufferIndex    = submesh->GetVertexPositionBuffer()->GetBindlessIndex();
        pc.VertexAttribBufferIndex = submesh->GetVertexAttributeBuffer()->GetBindlessIndex();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
            s_RendererData->ForwardRenderingPipeline, s_RendererData->ForwardRenderingPipeline->GetSpecification().Shader);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex = submesh->GetMeshletBuffer()->GetBindlessIndex();

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardRenderingPipeline, 0,
                                                                                               0, sizeof(pc), &pc);
            const uint32_t meshletCount = submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardRenderingPipeline, 0,
                                                                                               0, sizeof(pc), &pc);
            const uint64_t offsets[2] = {0, 0};
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{submesh->GetVertexPositionBuffer(), submesh->GetVertexAttributeBuffer()}, 0, 2, offsets);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(submesh->GetIndexBuffer());
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t));
        }
        s_RendererStats.TriangleCount += submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
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
    for (auto& submesh : mesh->GetSubmeshes())
    {
        if (submesh->IsOpaque())
            s_RendererData->OpaqueObjects.emplace_back(submesh);
        else
            s_RendererData->TransparentObjects.emplace_back(submesh);
    }
}

}  // namespace Pathfinder