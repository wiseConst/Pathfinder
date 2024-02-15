#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Core/Window.h"

#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Texture2D.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Material.h"
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
    SamplerStorage::Init();

    ShaderLibrary::Load(std::vector<std::string>{"pathtrace", "Forward", "DepthPrePass", "Utils/InfiniteGrid", "LightCulling", "Composite",
                                                 "AO/SSAO", "AO/SSAO_Blur"});

    std::ranges::for_each(s_RendererData->UploadHeap,
                          [](auto& uploadHeap)
                          {
                              BufferSpecification uploadHeapSpec = {EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE, true,
                                                                    s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY};
                              uploadHeap                         = Buffer::Create(uploadHeapSpec);
                          });

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS); });
    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });
    std::ranges::for_each(s_RendererData->TransferCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER); });

    {
        TextureSpecification whiteTextureSpec = {1, 1};
        uint32_t whiteColor                   = 0xFFFFFFFF;
        whiteTextureSpec.Data                 = &whiteColor;
        whiteTextureSpec.DataSize             = sizeof(whiteColor);
        s_RendererData->WhiteTexture          = Texture2D::Create(whiteTextureSpec);

        s_BindlessRenderer->LoadTexture(s_RendererData->WhiteTexture);
    }

    std::ranges::for_each(s_RendererData->CameraUB,
                          [](Shared<Buffer>& cameraUB)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_UNIFORM};
                              ubSpec.bMapPersistent      = true;
                              ubSpec.Data                = &s_RendererData->CameraStruct;
                              ubSpec.DataSize            = sizeof(s_RendererData->CameraStruct);

                              cameraUB = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->LightsUB,
                          [](Shared<Buffer>& lightsUB)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_UNIFORM};
                              ubSpec.bMapPersistent      = true;
                              ubSpec.Data                = &s_RendererData->LightStruct;
                              ubSpec.DataSize            = sizeof(s_RendererData->LightStruct);

                              lightsUB = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->CompositeFramebuffer,
                          [](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"Composite"};

                              const FramebufferAttachmentSpecification compositeAttachmentSpec = {
                                  EImageFormat::FORMAT_RGBA8_UNORM,  // /*EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32*/
                                  EImageLayout::IMAGE_LAYOUT_UNDEFINED, ELoadOp::CLEAR, EStoreOp::STORE, glm::vec4(0.15f), true};

                              framebufferSpec.Attachments.emplace_back(compositeAttachmentSpec);
                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height)
        { std::ranges::for_each(s_RendererData->CompositeFramebuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); }); });

    PipelineSpecification compositePipelineSpec = {"Composite", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    compositePipelineSpec.TargetFramebuffer     = s_RendererData->CompositeFramebuffer;
    compositePipelineSpec.Shader                = ShaderLibrary::Get("Composite");
    compositePipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    PipelineBuilder::Push(s_RendererData->CompositePipeline, compositePipelineSpec);

    std::ranges::for_each(s_RendererData->DepthPrePassFramebuffer,
                          [](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"DepthPrePass"};

                              const FramebufferAttachmentSpecification depthAttachmentSpec = {
                                  EImageFormat::FORMAT_D32F,
                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  ELoadOp::CLEAR,
                                  EStoreOp::STORE,

                                  glm::vec4(1.0f, glm::vec3(0.0f)),
                                  false};

                              framebufferSpec.Attachments.emplace_back(depthAttachmentSpec);
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

    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        FramebufferSpecification framebufferSpec = {"GBuffer"};

        // NOTE: For now I copy albedo into swapchain
        // Albedo
        FramebufferAttachmentSpecification attachmentSpec = {
            EImageFormat::FORMAT_RGBA8_UNORM, EImageLayout::IMAGE_LAYOUT_UNDEFINED, ELoadOp::CLEAR, EStoreOp::STORE, glm::vec4(1.0f), true};
        framebufferSpec.Attachments.emplace_back(attachmentSpec);

        // Depth from depth pre pass
        framebufferSpec.ExistingAttachments[1] = s_RendererData->DepthPrePassFramebuffer[frame]->GetDepthAttachment();
        attachmentSpec.LoadOp                  = ELoadOp::LOAD;
        attachmentSpec.StoreOp                 = EStoreOp::STORE;
        attachmentSpec.Format                  = EImageFormat::FORMAT_D32F;
        attachmentSpec.Wrap                    = ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE;
        framebufferSpec.Attachments.emplace_back(attachmentSpec);

        s_RendererData->GBuffer[frame] = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        { std::ranges::for_each(s_RendererData->GBuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); }); });

    PipelineSpecification forwardPipelineSpec  = {"Forward+", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    forwardPipelineSpec.Shader                 = ShaderLibrary::Get("Forward");
    forwardPipelineSpec.bBindlessCompatible    = true;
    forwardPipelineSpec.bSeparateVertexBuffers = true;
    forwardPipelineSpec.CullMode               = ECullMode::CULL_MODE_BACK;
    forwardPipelineSpec.bDepthTest             = true;
    forwardPipelineSpec.bDepthWrite =
        false;  // NOTE: Since I use depth from depth pre pass, I can not even write new depth, everything is done already
    forwardPipelineSpec.DepthCompareOp    = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
    forwardPipelineSpec.TargetFramebuffer = s_RendererData->GBuffer;
    forwardPipelineSpec.bMeshShading      = s_RendererSettings.bMeshShadingSupport;
    forwardPipelineSpec.bBlendEnable      = true;
    forwardPipelineSpec.BlendMode         = EBlendMode::BLEND_MODE_ALPHA;
    PipelineBuilder::Push(s_RendererData->ForwardPlusPipeline, forwardPipelineSpec);

    // TODO: Add depth only shader or somethin to handle grid
    PipelineSpecification gridPipelineSpec = {"InfiniteGrid", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    gridPipelineSpec.Shader                = ShaderLibrary::Get("Utils/InfiniteGrid");
    gridPipelineSpec.bBindlessCompatible   = true;
    gridPipelineSpec.TargetFramebuffer     = s_RendererData->GBuffer;
    PipelineBuilder::Push(s_RendererData->GridPipeline, gridPipelineSpec);

    Renderer2D::Init();

    /* vk_mini_path_tracer  */
    std::ranges::for_each(s_RendererData->PathtracedImage,
                          [](auto& image)
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

    PipelineSpecification pathTracingPipelineSpec = {"Pathtracing", EPipelineType::PIPELINE_TYPE_COMPUTE};
    pathTracingPipelineSpec.Shader                = ShaderLibrary::Get("pathtrace");
    pathTracingPipelineSpec.bBindlessCompatible   = true;
    PipelineBuilder::Push(s_RendererData->PathtracingPipeline, pathTracingPipelineSpec);
    /* vk_mini_path_tracer  */

    PipelineSpecification lightCullingPipelineSpec = {"LightCulling", EPipelineType::PIPELINE_TYPE_COMPUTE};
    lightCullingPipelineSpec.Shader                = ShaderLibrary::Get("LightCulling");
    lightCullingPipelineSpec.bBindlessCompatible   = true;
    PipelineBuilder::Push(s_RendererData->LightCullingPipeline, lightCullingPipelineSpec);

    std::ranges::for_each(s_RendererData->PointLightIndicesStorageBuffer,
                          [](auto& plIndicesSB)
                          {
                              BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE};

                              const uint32_t screenWidth  = s_RendererData->DepthPrePassFramebuffer[0]->GetSpecification().Width;
                              const uint32_t screenHeight = s_RendererData->DepthPrePassFramebuffer[0]->GetSpecification().Height;
                              const size_t sbSize         = MAX_POINT_LIGHTS * sizeof(uint32_t) * screenWidth * screenHeight /
                                                    LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE;
                              sbSpec.BufferCapacity = sbSize;

                              plIndicesSB = Buffer::Create(sbSpec);
                          });

    // TODO: Make use of it.
    std::ranges::for_each(s_RendererData->FrustumDebugImage,
                          [](auto& image)
                          {
                              ImageSpecification imageSpec = {};
                              imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
                              imageSpec.UsageFlags = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;

                              image = Image::Create(imageSpec);
                          });
    s_BindlessRenderer->LoadImage(s_RendererData->FrustumDebugImage);

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            std::ranges::for_each(s_RendererData->FrustumDebugImage, [&](auto& image) { image->Resize(width, height); });
            s_BindlessRenderer->LoadImage(s_RendererData->FrustumDebugImage);

            std::ranges::for_each(s_RendererData->PointLightIndicesStorageBuffer,
                                  [&](auto& plIndicesSB)
                                  {
                                      const size_t sbSize = MAX_POINT_LIGHTS * sizeof(uint32_t) * width * height / LIGHT_CULLING_TILE_SIZE /
                                                            LIGHT_CULLING_TILE_SIZE;

                                      plIndicesSB->Resize(sbSize);
                                  });

            s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                                 s_RendererData->PointLightIndicesStorageBuffer);

            s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                                s_RendererData->PointLightIndicesStorageBuffer);

            ImagePerFrame depthAttachments;
            for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
                depthAttachments[frame] = s_RendererData->DepthPrePassFramebuffer[frame]->GetDepthAttachment();

            s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("u_DepthTex", depthAttachments);
        });

    std::ranges::for_each(s_RendererData->SSAOFramebuffer,
                          [](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"SSAO"};

                              const FramebufferAttachmentSpecification ssaoAttachmentSpec = {
                                  EImageFormat::FORMAT_R8_UNORM,
                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  ELoadOp::CLEAR,
                                  EStoreOp::STORE,
                                  glm::vec4(1.0f, glm::vec3(0.0f)),
                                  false};

                              framebufferSpec.Attachments.emplace_back(ssaoAttachmentSpec);
                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            std::ranges::for_each(s_RendererData->SSAOFramebuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); });

            ImagePerFrame depthAttachments;
            for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
                depthAttachments[frame] = s_RendererData->DepthPrePassFramebuffer[frame]->GetDepthAttachment();

            s_RendererData->SSAOPipeline->GetSpecification().Shader->Set("u_DepthTex", depthAttachments);
        });

    PipelineSpecification ssaoPipelineSpec = {"SSAO", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    ssaoPipelineSpec.TargetFramebuffer     = s_RendererData->SSAOFramebuffer;
    ssaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO");
    ssaoPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    ssaoPipelineSpec.bBindlessCompatible   = true;
    PipelineBuilder::Push(s_RendererData->SSAOPipeline, ssaoPipelineSpec);

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::mt19937_64 rndEngine;
    std::vector<glm::vec3> ssaoNoise;
    for (uint32_t i{}; i < 16; ++i)
    {
        glm::vec3 noise(randomFloats(rndEngine) * 2.0 - 1.0, randomFloats(rndEngine) * 2.0 - 1.0, 0.0f);
        ssaoNoise.push_back(noise);
    }
    TextureSpecification ssaoNoiseTextureSpec = {4, 4};
    ssaoNoiseTextureSpec.Data                 = ssaoNoise.data();
    ssaoNoiseTextureSpec.DataSize             = ssaoNoise.size() * sizeof(ssaoNoise[0]);
    ssaoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_NEAREST;
    ssaoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ssaoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA16F;
    s_RendererData->SSAONoiseTexture          = Texture2D::Create(ssaoNoiseTextureSpec);

    std::ranges::for_each(s_RendererData->SSAOBlurFramebuffer,
                          [](auto& framebuffer)
                          {
                              FramebufferSpecification framebufferSpec = {"SSAO_Blur"};

                              const FramebufferAttachmentSpecification ssaoBlurAttachmentSpec = {
                                  EImageFormat::FORMAT_R8_UNORM,
                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  ELoadOp::CLEAR,
                                  EStoreOp::STORE,
                                  glm::vec4(1.0f, glm::vec3(0.0f)),
                                  false};

                              framebufferSpec.Attachments.emplace_back(ssaoBlurAttachmentSpec);
                              framebuffer = Framebuffer::Create(framebufferSpec);
                          });

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            std::ranges::for_each(s_RendererData->SSAOBlurFramebuffer, [&](auto& framebuffer) { framebuffer->Resize(width, height); });

            {
                ImagePerFrame ssaoAttachments;
                for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
                    ssaoAttachments[frame] = s_RendererData->SSAOFramebuffer[frame]->GetAttachments()[0].Attachment;

                s_RendererData->SSAOBlurPipeline->GetSpecification().Shader->Set("u_SSAO", ssaoAttachments);
            }

            {
                ImagePerFrame ssaoBlurredAttachments;
                for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
                    ssaoBlurredAttachments[frame] = s_RendererData->SSAOBlurFramebuffer[frame]->GetAttachments()[0].Attachment;

                s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("u_SSAO", ssaoBlurredAttachments);
            }
        });

    PipelineSpecification ssaoBlurPipelineSpec = {"SSAO_Blur", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    ssaoBlurPipelineSpec.TargetFramebuffer     = s_RendererData->SSAOBlurFramebuffer;
    ssaoBlurPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO_Blur");
    ssaoBlurPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    PipelineBuilder::Push(s_RendererData->SSAOBlurPipeline, ssaoBlurPipelineSpec);

    /* CREATE STAGE */
    PipelineBuilder::Build();
    ShaderLibrary::DestroyGarbageIfNeeded();
    /* SHADER UPDATE STAGE */

    s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                         s_RendererData->PointLightIndicesStorageBuffer);

    s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                        s_RendererData->PointLightIndicesStorageBuffer);

    ImagePerFrame depthAttachments;
    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
        depthAttachments[frame] = s_RendererData->DepthPrePassFramebuffer[frame]->GetDepthAttachment();

    s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("u_DepthTex", depthAttachments);

    s_RendererData->SSAOPipeline->GetSpecification().Shader->Set("u_DepthTex", depthAttachments);
    s_RendererData->SSAOPipeline->GetSpecification().Shader->Set("u_NoiseTex", s_RendererData->SSAONoiseTexture);

    {
        ImagePerFrame ssaoAttachments;
        for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
            ssaoAttachments[frame] = s_RendererData->SSAOFramebuffer[frame]->GetAttachments()[0].Attachment;

        s_RendererData->SSAOBlurPipeline->GetSpecification().Shader->Set("u_SSAO", ssaoAttachments);
    }

    {
        ImagePerFrame ssaoBlurredAttachments;
        for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
            ssaoBlurredAttachments[frame] = s_RendererData->SSAOBlurFramebuffer[frame]->GetAttachments()[0].Attachment;

        s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("u_SSAO", ssaoBlurredAttachments);
    }

    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D created!");
}

void Renderer::Shutdown()
{
    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();
    PipelineBuilder::Shutdown();

    s_RendererData.reset();
    s_BindlessRenderer.reset();

    SamplerStorage::Shutdown();
    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D destroyed!");
}

void Renderer::Begin()
{
    s_RendererData->FrameIndex                   = Application::Get().GetWindow()->GetCurrentFrameIndex();
    s_RendererData->CurrentRenderCommandBuffer   = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentComputeCommandBuffer  = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentTransferCommandBuffer = s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex];

    s_RendererData->UploadHeap[s_RendererData->FrameIndex]->Resize(s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY);
    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    s_BindlessRenderer->UpdateDataIfNeeded();

    uint32_t prevPoolCount              = s_RendererStats.DescriptorPoolCount;
    uint32_t prevDescriptorSetCount     = s_RendererStats.DescriptorSetCount;
    s_RendererStats                     = {};
    s_RendererStats.DescriptorPoolCount = prevPoolCount;
    s_RendererStats.DescriptorSetCount  = prevDescriptorSetCount;

    s_RendererData->CameraStruct = {};
    s_RendererData->LightStruct  = {};

    Renderer2D::GetRendererData()->FrameIndex = s_RendererData->FrameIndex;
    Renderer2D::Begin();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
}

void Renderer::Flush()
{
    s_RendererData->LightsUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->LightStruct, sizeof(s_RendererData->LightStruct));
    s_BindlessRenderer->UpdateLightData(s_RendererData->LightsUB[s_RendererData->FrameIndex]);

    s_BindlessRenderer->UpdateDataIfNeeded();

    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    /* PATHTRACING TESTS from vk_mini_path_tracer */
    if (s_RendererSettings.bRTXSupport)
    {
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
            s_RendererData->PathtracingPipeline->GetSpecification().DebugName, glm::vec3(0.8f, 0.8f, 0.1f));

        PushConstantBlock pc = {};
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

    std::sort(s_RendererData->OpaqueObjects.begin(), s_RendererData->OpaqueObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::abs(glm::length(lhsPos - s_RendererData->CameraStruct.Position));
                  const float rhsDist = glm::abs(glm::length(rhsPos - s_RendererData->CameraStruct.Position));

                  return lhsDist < rhsDist;  // Front to back drawing(minimize overdraw)
              });

    std::sort(s_RendererData->TransparentObjects.begin(), s_RendererData->TransparentObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::abs(glm::length(lhsPos - s_RendererData->CameraStruct.Position));
                  const float rhsDist = glm::abs(glm::length(rhsPos - s_RendererData->CameraStruct.Position));

                  return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
              });

    DepthPrePass();
    // NOTE: Insert shadow-passes

    SSAOPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    LightCullingPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    GeometryPass();

    Renderer2D::Flush();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    // TODO: Sort out timeline semaphores
    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->Submit(true, false);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit(true, false);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit(
        true, false/*, EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT,
        {s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->GetWaitSemaphore()}, 1, &signalValue*/);

    const auto& renderTimestamps = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetTimestampsResults();
    s_RendererStats.GPUTime      = std::accumulate(renderTimestamps.begin(), renderTimestamps.end(), s_RendererStats.GPUTime);

#define LOG_RENDERER 0

#if LOG_RENDERER
    const auto& renderPipelineStats  = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();
    const auto& computePipelineStats = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();

    LOG_TAG_INFO(RENDERER, "RENDERER STATISTICS: ");
    LOG_TAG_INFO(SAMPLER_STORAGE, "Sampler count: %u", SamplerStorage::GetSamplerCount());
    LOG_TAG_INFO(RENDERER, "DepthPrePass: %0.4f (ms)", renderTimestamps[0]);
    LOG_TAG_INFO(RENDERER, "SSAOPass: %0.4f (ms)", renderTimestamps[1]);
    LOG_TAG_INFO(RENDERER, "SSAOBlurPass: %0.4f (ms)", renderTimestamps[2]);
    LOG_TAG_INFO(RENDERER, "LightCullingPass: %0.4f (ms)", renderTimestamps[3]);
    LOG_TAG_INFO(RENDERER, "GeometryPass: %0.4f (ms)", renderTimestamps[4]);
    for (const auto& renderPipelineStat : renderPipelineStats)
    {
        LOG_TAG_INFO(RENDERER, "%s %llu", CommandBuffer::ConvertQueryPipelineStatisticToCString(renderPipelineStat.first),
                     renderPipelineStat.second);
    }
#endif

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();
    s_RendererData->CurrentTransferCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    //  Application::Get().GetWindow()->CopyToWindow(s_RendererData->PathtracedImage[s_RendererData->FrameIndex]);
    // Application::Get().GetWindow()->CopyToWindow(s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetAttachments()[0].Attachment);
    Application::Get().GetWindow()->CopyToWindow(s_RendererData->GBuffer[s_RendererData->FrameIndex]->GetAttachments()[0].Attachment);
}

void Renderer::BeginScene(const Camera& camera)
{
    s_RendererData->CameraStruct = {camera.GetProjection(),
                                    camera.GetView(),
                                    camera.GetViewProjection(),
                                    camera.GetInverseProjection(),
                                    camera.GetPosition(),
                                    camera.GetNearPlaneDepth(),
                                    camera.GetFarPlaneDepth(),
                                    glm::vec2(s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetSpecification().Width,
                                              s_RendererData->CompositeFramebuffer[s_RendererData->FrameIndex]->GetSpecification().Height)};

    s_RendererData->CameraUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraStruct, sizeof(s_RendererData->CameraStruct));
    s_BindlessRenderer->UpdateCameraData(s_RendererData->CameraUB[s_RendererData->FrameIndex]);
}

void Renderer::EndScene() {}

void Renderer::DrawGrid()
{
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0));

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->GridPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(6);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::DepthPrePass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.2f, 0.2f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->BeginPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->DepthPrePassPipeline);

    auto renderMeshFunc = [&](const RenderObject& renderObject)
    {
        PushConstantBlock pc = {s_RendererData->CameraStruct.ViewProjection * renderObject.Transform};  // NOTE: CPU mat mult > GPU mat mult
        pc.VertexPosBufferIndex = renderObject.submesh->GetVertexPositionBuffer()->GetBindlessIndex();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
            s_RendererData->DepthPrePassPipeline, s_RendererData->DepthPrePassPipeline->GetSpecification().Shader);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex          = renderObject.submesh->GetMeshletBuffer()->GetBindlessIndex();
            pc.MeshletVerticesBufferIndex  = renderObject.submesh->GetMeshletVerticesBuffer()->GetBindlessIndex();
            pc.MeshletTrianglesBufferIndex = renderObject.submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex();

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            const uint64_t offset = 0;
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{renderObject.submesh->GetVertexPositionBuffer()}, 0, 1, &offset);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(renderObject.submesh->GetIndexBuffer());
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t));
        }
        s_RendererStats.TriangleCount += renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
    };

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));
    for (auto& opaque : s_RendererData->OpaqueObjects)
    {
        renderMeshFunc(opaque);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    // TODO: Should I depth pre pass transparent?? (it breaks alpha-blending)
    /*s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    for (auto& transparent : s_RendererData->TransparentObjects)
    {
        renderMeshFunc(transparent);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();*/

    s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->EndPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::SSAOPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->SSAOFramebuffer[s_RendererData->FrameIndex]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->SSAOPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
        s_RendererData->SSAOPipeline, s_RendererData->SSAOPipeline->GetSpecification().Shader);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->SSAOFramebuffer[s_RendererData->FrameIndex]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    // BOX-BLURRING

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("SSAO_Blur");
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->SSAOBlurFramebuffer[s_RendererData->FrameIndex]->BeginPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->SSAOBlurPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
        s_RendererData->SSAOBlurPipeline, s_RendererData->SSAOBlurPipeline->GetSpecification().Shader);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->SSAOBlurFramebuffer[s_RendererData->FrameIndex]->EndPass(
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::LightCullingPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.9f, 0.9f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    PushConstantBlock pc  = {};
    pc.AlbedoTextureIndex = s_RendererData->FrustumDebugImage[s_RendererData->FrameIndex]->GetBindlessIndex();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->LightCullingPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
        s_RendererData->LightCullingPipeline, s_RendererData->LightCullingPipeline->GetSpecification().Shader);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->LightCullingPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        (s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->GetSpecification().Width + LIGHT_CULLING_TILE_SIZE - 1) /
            LIGHT_CULLING_TILE_SIZE,
        (s_RendererData->DepthPrePassFramebuffer[s_RendererData->FrameIndex]->GetSpecification().Height + LIGHT_CULLING_TILE_SIZE - 1) /
            LIGHT_CULLING_TILE_SIZE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::GeometryPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.9f, 0.5f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->GBuffer[s_RendererData->FrameIndex]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    DrawGrid();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->ForwardPlusPipeline);

    auto renderMeshFunc = [&](const RenderObject& renderObject)
    {
        PushConstantBlock pc             = {renderObject.Transform};
        pc.VertexPosBufferIndex          = renderObject.submesh->GetVertexPositionBuffer()->GetBindlessIndex();
        pc.VertexAttribBufferIndex       = renderObject.submesh->GetVertexAttributeBuffer()->GetBindlessIndex();
        pc.AlbedoTextureIndex            = renderObject.submesh->GetMaterial()->GetAlbedoIndex();
        pc.NormalTextureIndex            = renderObject.submesh->GetMaterial()->GetNormalMapIndex();
        pc.MetallicRoughnessTextureIndex = renderObject.submesh->GetMaterial()->GetMetallicRoughnessIndex();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindShaderData(
            s_RendererData->ForwardPlusPipeline, s_RendererData->ForwardPlusPipeline->GetSpecification().Shader);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex          = renderObject.submesh->GetMeshletBuffer()->GetBindlessIndex();
            pc.MeshletVerticesBufferIndex  = renderObject.submesh->GetMeshletVerticesBuffer()->GetBindlessIndex();
            pc.MeshletTrianglesBufferIndex = renderObject.submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex();

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardPlusPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);
            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardPlusPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);
            const uint64_t offsets[2] = {0, 0};
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{renderObject.submesh->GetVertexPositionBuffer(),
                                            renderObject.submesh->GetVertexAttributeBuffer()},
                0, 2, offsets);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(renderObject.submesh->GetIndexBuffer());
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t));
        }
        s_RendererStats.TriangleCount += renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
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

    s_RendererData->GBuffer[s_RendererData->FrameIndex]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::SubmitMesh(const Shared<Mesh>& mesh, const glm::mat4& transform)
{
    for (auto& submesh : mesh->GetSubmeshes())
    {
        if (submesh->GetMaterial()->IsOpaque())
            s_RendererData->OpaqueObjects.emplace_back(submesh, transform);
        else
            s_RendererData->TransparentObjects.emplace_back(submesh, transform);
    }
}

void Renderer::AddDirectionalLight(const DirectionalLight& dl)
{
    if (s_RendererData->LightStruct.DirectionalLightCount + 1 > MAX_DIR_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max directional light limit reached!");
        return;
    }

    s_RendererData->LightStruct.DirectionalLights[s_RendererData->LightStruct.DirectionalLightCount++] = dl;
}

void Renderer::AddPointLight(const PointLight& pl)
{
    if (s_RendererData->LightStruct.PointLightCount + 1 > MAX_POINT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max point light limit reached!");
        return;
    }

    s_RendererData->LightStruct.PointLights[s_RendererData->LightStruct.PointLightCount++] = pl;
}

void Renderer::AddSpotLight(const SpotLight& sl)
{
    if (s_RendererData->LightStruct.SpotLightCount + 1 > MAX_SPOT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max spot light limit reached!");
        return;
    }

    s_RendererData->LightStruct.SpotLights[s_RendererData->LightStruct.SpotLightCount++] = sl;
}

}  // namespace Pathfinder