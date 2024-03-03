#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Swapchain.h"
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

// TODO: Integrate fucking std::tie

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
                                                 "AO/SSAO", "AO/HBAO", "AO/AO_Blur", "Shadows/DirShadowMap"});

    std::ranges::for_each(s_RendererData->UploadHeap,
                          [](auto& uploadHeap)
                          {
                              BufferSpecification uploadHeapSpec = {EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE, true,
                                                                    s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY};

                              uploadHeap = Buffer::Create(uploadHeapSpec);
                          });

    std::ranges::for_each(s_RendererData->CameraUB,
                          [](Shared<Buffer>& cameraUB)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_UNIFORM};
                              ubSpec.bMapPersistent      = true;
                              ubSpec.Data                = &s_RendererData->CameraStruct;
                              ubSpec.DataSize            = sizeof(s_RendererData->CameraStruct);

                              cameraUB = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->LightsSSBO,
                          [](Shared<Buffer>& lightsSSBO)

                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_STORAGE};
                              ubSpec.bMapPersistent      = true;
                              ubSpec.Data                = &s_RendererData->LightStruct;
                              ubSpec.DataSize            = sizeof(s_RendererData->LightStruct);

                              lightsSSBO = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->RenderCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, true); });
    std::ranges::for_each(s_RendererData->ComputeCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE); });
    std::ranges::for_each(s_RendererData->TransferCommandBuffer, [](auto& commandBuffer)
                          { commandBuffer = CommandBuffer::Create(ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER); });

    // Submit render fences to make use of frames in flight(needs better handling in future)
    {
        std::array<void*, s_FRAMES_IN_FLIGHT> fences;
        std::array<void*, s_FRAMES_IN_FLIGHT> semaphores;
        for (uint32_t i{}; i < s_FRAMES_IN_FLIGHT; ++i)
        {
            fences[i]     = s_RendererData->RenderCommandBuffer[i]->GetSubmitFence();
            semaphores[i] = s_RendererData->RenderCommandBuffer[i]->GetWaitSemaphore();
        }

        Application::Get().GetWindow()->GetSwapchain()->SetRenderFence(fences);
        Application::Get().GetWindow()->GetSwapchain()->SetWaitSemaphore(semaphores);
    }

    {
        TextureSpecification whiteTextureSpec = {1, 1};
        uint32_t whiteColor                   = 0xFFFFFFFF;
        whiteTextureSpec.Data                 = &whiteColor;
        whiteTextureSpec.DataSize             = sizeof(whiteColor);
        s_RendererData->WhiteTexture          = Texture2D::Create(whiteTextureSpec);

        s_BindlessRenderer->LoadTexture(s_RendererData->WhiteTexture);
    }

    {
        FramebufferSpecification framebufferSpec = {"Composite"};

        const FramebufferAttachmentSpecification compositeAttachmentSpec = {EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32,
                                                                            EImageLayout::IMAGE_LAYOUT_UNDEFINED,
                                                                            ELoadOp::CLEAR,
                                                                            EStoreOp::STORE,
                                                                            glm::vec4(0.15f),
                                                                            true};

        framebufferSpec.Attachments.emplace_back(compositeAttachmentSpec);
        s_RendererData->CompositeFramebuffer = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                      { s_RendererData->CompositeFramebuffer->Resize(width, height); });

    PipelineSpecification compositePipelineSpec = {"Composite", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    compositePipelineSpec.TargetFramebuffer     = s_RendererData->CompositeFramebuffer;
    compositePipelineSpec.Shader                = ShaderLibrary::Get("Composite");
    compositePipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    PipelineBuilder::Push(s_RendererData->CompositePipeline, compositePipelineSpec);

    {
        FramebufferSpecification framebufferSpec = {"DepthPrePass"};

        const FramebufferAttachmentSpecification depthAttachmentSpec = {EImageFormat::FORMAT_D32F,
                                                                        EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                        ELoadOp::CLEAR,
                                                                        EStoreOp::STORE,
                                                                        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
                                                                        false};

        framebufferSpec.Attachments.emplace_back(depthAttachmentSpec);
        s_RendererData->DepthPrePassFramebuffer = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                      { s_RendererData->DepthPrePassFramebuffer->Resize(width, height); });

    // NOTE: Reversed-Z, summary:https://ajweeks.com/blog/2019/04/06/ReverseZ/
    PipelineSpecification depthPrePassPipelineSpec = {"DepthPrePass", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    depthPrePassPipelineSpec.Shader                = ShaderLibrary::Get("DepthPrePass");
    depthPrePassPipelineSpec.bBindlessCompatible   = true;
    depthPrePassPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    depthPrePassPipelineSpec.TargetFramebuffer     = s_RendererData->DepthPrePassFramebuffer;
    depthPrePassPipelineSpec.bMeshShading          = s_RendererSettings.bMeshShadingSupport;
    depthPrePassPipelineSpec.bDepthTest            = true;
    depthPrePassPipelineSpec.bDepthWrite           = true;
    depthPrePassPipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    PipelineBuilder::Push(s_RendererData->DepthPrePassPipeline, depthPrePassPipelineSpec);

    {
        FramebufferSpecification framebufferSpec = {"GBuffer"};

        // NOTE: For now I copy albedo into swapchain
        // Albedo
        FramebufferAttachmentSpecification attachmentSpec = {
            EImageFormat::FORMAT_RGBA32F, EImageLayout::IMAGE_LAYOUT_UNDEFINED, ELoadOp::CLEAR, EStoreOp::STORE, glm::vec4(.0f), true};
        framebufferSpec.Attachments.emplace_back(attachmentSpec);

        // Depth from depth pre pass
        framebufferSpec.ExistingAttachments[1] = s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment();
        attachmentSpec.LoadOp                  = ELoadOp::LOAD;
        attachmentSpec.StoreOp                 = EStoreOp::STORE;
        attachmentSpec.Format                  = framebufferSpec.ExistingAttachments[1]->GetSpecification().Format;
        attachmentSpec.ClearColor              = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].Specification.ClearColor;
        framebufferSpec.Attachments.emplace_back(attachmentSpec);

        s_RendererData->GBuffer = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                      { s_RendererData->GBuffer->Resize(width, height); });

    PipelineSpecification forwardPipelineSpec  = {"ForwardPlus", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    forwardPipelineSpec.Shader                 = ShaderLibrary::Get("Forward");
    forwardPipelineSpec.bBindlessCompatible    = true;
    forwardPipelineSpec.bSeparateVertexBuffers = true;
    forwardPipelineSpec.CullMode               = ECullMode::CULL_MODE_BACK;
    forwardPipelineSpec.bDepthTest             = true;
    forwardPipelineSpec.bDepthWrite            = false;
    forwardPipelineSpec.DepthCompareOp         = depthPrePassPipelineSpec.DepthCompareOp;
    forwardPipelineSpec.TargetFramebuffer      = s_RendererData->GBuffer;
    forwardPipelineSpec.bMeshShading           = s_RendererSettings.bMeshShadingSupport;
    forwardPipelineSpec.bBlendEnable           = true;
    forwardPipelineSpec.BlendMode              = EBlendMode::BLEND_MODE_ALPHA;
    PipelineBuilder::Push(s_RendererData->ForwardPlusPipeline, forwardPipelineSpec);

    // TODO: Add depth only shader or somethin to handle grid
    PipelineSpecification gridPipelineSpec = {"InfiniteGrid", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    gridPipelineSpec.Shader                = ShaderLibrary::Get("Utils/InfiniteGrid");
    gridPipelineSpec.bBindlessCompatible   = true;
    gridPipelineSpec.TargetFramebuffer     = s_RendererData->GBuffer;
    PipelineBuilder::Push(s_RendererData->GridPipeline, gridPipelineSpec);

    Renderer2D::Init();

    /* vk_mini_path_tracer  */
    {
        ImageSpecification imageSpec = {};
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;

        s_RendererData->PathtracedImage = Image::Create(imageSpec);
    }
    s_BindlessRenderer->LoadImage(s_RendererData->PathtracedImage);

    Application::Get().GetWindow()->AddResizeCallback(
        [&](uint32_t width, uint32_t height)
        {
            s_RendererData->PathtracedImage->Resize(width, height);
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

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE};

        const uint32_t screenWidth  = s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width;
        const uint32_t screenHeight = s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height;
        const size_t sbSize =
            MAX_POINT_LIGHTS * sizeof(uint32_t) * screenWidth * screenHeight / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE;
        sbSpec.BufferCapacity = sbSize;

        s_RendererData->PointLightIndicesStorageBuffer = Buffer::Create(sbSpec);
    }

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE};

        const uint32_t screenWidth  = s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width;
        const uint32_t screenHeight = s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height;
        const size_t sbSize =
            MAX_SPOT_LIGHTS * sizeof(uint32_t) * screenWidth * screenHeight / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE;
        sbSpec.BufferCapacity = sbSize;

        s_RendererData->SpotLightIndicesStorageBuffer = Buffer::Create(sbSpec);
    }

    {
        ImageSpecification imageSpec = {};
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        s_RendererData->FrustumDebugImage = Image::Create(imageSpec);
        s_BindlessRenderer->LoadImage(s_RendererData->FrustumDebugImage);
    }

    {
        ImageSpecification imageSpec = {};
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        s_RendererData->LightHeatMapImage = Image::Create(imageSpec);
        s_BindlessRenderer->LoadImage(s_RendererData->LightHeatMapImage);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            s_RendererData->FrustumDebugImage->Resize(width, height);
            s_BindlessRenderer->LoadImage(s_RendererData->FrustumDebugImage);

            s_RendererData->LightHeatMapImage->Resize(width, height);
            s_BindlessRenderer->LoadImage(s_RendererData->LightHeatMapImage);

            {
                const size_t sbSize =
                    MAX_POINT_LIGHTS * sizeof(uint32_t) * width * height / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE;

                s_RendererData->PointLightIndicesStorageBuffer->Resize(sbSize);
            }

            {
                const size_t sbSize =
                    MAX_SPOT_LIGHTS * sizeof(uint32_t) * width * height / LIGHT_CULLING_TILE_SIZE / LIGHT_CULLING_TILE_SIZE;

                s_RendererData->SpotLightIndicesStorageBuffer->Resize(sbSize);
            }

            s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                                 s_RendererData->PointLightIndicesStorageBuffer);

            s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                                s_RendererData->PointLightIndicesStorageBuffer);

            s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisibleSpotLightIndicesBuffer",
                                                                                 s_RendererData->SpotLightIndicesStorageBuffer);

            s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisibleSpotLightIndicesBuffer",
                                                                                s_RendererData->SpotLightIndicesStorageBuffer);

            s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("u_DepthTex",
                                                                                 s_RendererData->GBuffer->GetDepthAttachment());
        });

    {
        FramebufferSpecification framebufferSpec = {"SSAO"};

        const FramebufferAttachmentSpecification ssaoAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                       EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                       ELoadOp::CLEAR,
                                                                       EStoreOp::STORE,
                                                                       glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                       false,
                                                                       ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                       ESamplerFilter::SAMPLER_FILTER_LINEAR};

        framebufferSpec.Attachments.emplace_back(ssaoAttachmentSpec);
        s_RendererData->SSAO.Framebuffer = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            s_RendererData->SSAO.Framebuffer->Resize(width, height);

            s_RendererData->SSAO.Pipeline->GetSpecification().Shader->Set("u_DepthTex",
                                                                          s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment());
        });

    PipelineSpecification ssaoPipelineSpec = {"SSAO", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    ssaoPipelineSpec.TargetFramebuffer     = s_RendererData->SSAO.Framebuffer;
    ssaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO");
    ssaoPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    ssaoPipelineSpec.bBindlessCompatible   = true;
    PipelineBuilder::Push(s_RendererData->SSAO.Pipeline, ssaoPipelineSpec);

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::mt19937_64 rndEngine;
    std::vector<glm::vec3> ssaoNoise;
    for (uint32_t i{}; i < 16; ++i)
    {
        ssaoNoise.emplace_back(randomFloats(rndEngine) * 2.0 - 1.0, randomFloats(rndEngine) * 2.0 - 1.0, 0.0f);
    }
    TextureSpecification ssaoNoiseTextureSpec = {4, 4};
    ssaoNoiseTextureSpec.Data                 = ssaoNoise.data();
    ssaoNoiseTextureSpec.DataSize             = ssaoNoise.size() * sizeof(ssaoNoise[0]);
    ssaoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_NEAREST;
    ssaoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ssaoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA32F;
    s_RendererData->AONoiseTexture            = Texture2D::Create(ssaoNoiseTextureSpec);

    {
        FramebufferSpecification framebufferSpec = {"SSAO_Blur"};

        const FramebufferAttachmentSpecification ssaoBlurAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                           EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                           ELoadOp::CLEAR,
                                                                           EStoreOp::STORE,
                                                                           glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                           false,
                                                                           ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                           ESamplerFilter::SAMPLER_FILTER_LINEAR};

        framebufferSpec.Attachments.emplace_back(ssaoBlurAttachmentSpec);
        s_RendererData->SSAOBlurFramebuffer = Framebuffer::Create(framebufferSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            s_RendererData->SSAOBlurFramebuffer->Resize(width, height);

            s_RendererData->SSAOBlurPipeline->GetSpecification().Shader->Set(
                "u_AO", s_RendererData->SSAO.Framebuffer->GetAttachments()[0].Attachment);

            s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set(
                "u_SSAO", s_RendererData->SSAOBlurFramebuffer->GetAttachments()[0].Attachment);
        });

    PipelineSpecification ssaoBlurPipelineSpec = {"SSAO_Blur", EPipelineType::PIPELINE_TYPE_GRAPHICS};
    ssaoBlurPipelineSpec.TargetFramebuffer     = s_RendererData->SSAOBlurFramebuffer;
    ssaoBlurPipelineSpec.Shader                = ShaderLibrary::Get("AO/AO_Blur");
    ssaoBlurPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
    PipelineBuilder::Push(s_RendererData->SSAOBlurPipeline, ssaoBlurPipelineSpec);

    {
        // Directional Shadow Mapping
        s_RendererData->DirShadowMaps.resize(MAX_DIR_LIGHTS);
        for (uint32_t i{}; i < s_RendererData->DirShadowMaps.size(); ++i)
        {
            std::string framebufferDebugName                     = std::string("DirShadowMap_") + std::to_string(i);
            FramebufferSpecification dirShadowMapFramebufferSpec = {framebufferDebugName.data()};

            dirShadowMapFramebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_D32F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(1.0f), false, ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_BORDER);

            s_RendererData->DirShadowMaps[i] = Framebuffer::Create(dirShadowMapFramebufferSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height)
            {
                for (const auto& dirShadowMap : s_RendererData->DirShadowMaps)
                    dirShadowMap->Resize(width, height);

                std::vector<Shared<Image>> attachments(MAX_DIR_LIGHTS);
                for (size_t i{}; i < attachments.size(); ++i)
                    attachments[i] = s_RendererData->DirShadowMaps[i]->GetDepthAttachment();

                s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("u_DirShadowmap", attachments);

                // To recalculate dir shadow maps view projection matrices
                s_RendererData->LightStruct = {};
            });

        PFR_ASSERT(!s_RendererData->DirShadowMaps.empty() && MAX_DIR_LIGHTS > 0, "Failed to create directional shadow map framebuffers!");
        PipelineSpecification dirShadowMapPipelineSpec = {"DirShadowMap", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        dirShadowMapPipelineSpec.Shader                = ShaderLibrary::Get("Shadows/DirShadowMap");
        dirShadowMapPipelineSpec.TargetFramebuffer     = s_RendererData->DirShadowMaps[0];
        dirShadowMapPipelineSpec.bBindlessCompatible   = true;
        dirShadowMapPipelineSpec.bMeshShading          = s_RendererSettings.bMeshShadingSupport;
        dirShadowMapPipelineSpec.CullMode              = ECullMode::CULL_MODE_FRONT;
        dirShadowMapPipelineSpec.bDepthTest            = true;
        dirShadowMapPipelineSpec.bDepthWrite           = true;
        dirShadowMapPipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
        PipelineBuilder::Push(s_RendererData->DirShadowMapPipeline, dirShadowMapPipelineSpec);
    }

    /* CREATE STAGE */
    PipelineBuilder::Build();
    ShaderLibrary::DestroyGarbageIfNeeded();
    /* SHADER UPDATE STAGE */

    s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                         s_RendererData->PointLightIndicesStorageBuffer);

    s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisiblePointLightIndicesBuffer",
                                                                        s_RendererData->PointLightIndicesStorageBuffer);

    s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("s_VisibleSpotLightIndicesBuffer",
                                                                         s_RendererData->SpotLightIndicesStorageBuffer);

    s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("s_VisibleSpotLightIndicesBuffer",
                                                                        s_RendererData->SpotLightIndicesStorageBuffer);

    s_RendererData->LightCullingPipeline->GetSpecification().Shader->Set("u_DepthTex", s_RendererData->GBuffer->GetDepthAttachment());

    s_RendererData->SSAO.Pipeline->GetSpecification().Shader->Set("u_DepthTex",
                                                                  s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment());
    s_RendererData->SSAO.Pipeline->GetSpecification().Shader->Set("u_NoiseTex", s_RendererData->AONoiseTexture);

    s_RendererData->SSAOBlurPipeline->GetSpecification().Shader->Set("u_AO",
                                                                     s_RendererData->SSAO.Framebuffer->GetAttachments()[0].Attachment);

    s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set(
        "u_SSAO", s_RendererData->SSAOBlurFramebuffer->GetAttachments()[0].Attachment);

    {
        std::vector<Shared<Image>> attachments(s_RendererData->DirShadowMaps.size());
        for (size_t i{}; i < attachments.size(); ++i)
            attachments[i] = s_RendererData->DirShadowMaps[i]->GetDepthAttachment();

        s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("u_DirShadowmap", attachments);
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
    s_RendererData->LastBoundPipeline.reset();

    s_RendererData->FrameIndex                   = Application::Get().GetWindow()->GetCurrentFrameIndex();
    s_RendererData->CurrentRenderCommandBuffer   = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentComputeCommandBuffer  = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex];
    s_RendererData->CurrentTransferCommandBuffer = s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex];

    s_RendererData->UploadHeap[s_RendererData->FrameIndex]->Resize(s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY);
    s_BindlessRenderer->UpdateDataIfNeeded();

    uint32_t prevPoolCount              = s_RendererStats.DescriptorPoolCount;
    uint32_t prevDescriptorSetCount     = s_RendererStats.DescriptorSetCount;
    s_RendererStats                     = {};
    s_RendererStats.DescriptorPoolCount = prevPoolCount;
    s_RendererStats.DescriptorSetCount  = prevDescriptorSetCount;

    s_RendererData->CameraStruct                      = {};
    s_RendererData->LightStruct.DirectionalLightCount = 0;
    s_RendererData->LightStruct.SpotLightCount        = 0;
    s_RendererData->LightStruct.PointLightCount       = 0;

    {
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
    }

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    Renderer2D::GetRendererData()->FrameIndex = s_RendererData->FrameIndex;
    Renderer2D::Begin();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
}

void Renderer::Flush()
{
    s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->SetData(&s_RendererData->LightStruct, sizeof(s_RendererData->LightStruct));
    s_BindlessRenderer->UpdateLightData(s_RendererData->LightsSSBO[s_RendererData->FrameIndex]);

    s_BindlessRenderer->UpdateDataIfNeeded();

    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    /* PATHTRACING TESTS from vk_mini_path_tracer */
    if (s_RendererSettings.bRTXSupport)
    {
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
            s_RendererData->PathtracingPipeline->GetSpecification().DebugName);

        PushConstantBlock pc = {};
        pc.StorageImageIndex = s_RendererData->PathtracedImage->GetBindlessIndex();
        BindPipeline(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex], s_RendererData->PathtracingPipeline);
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->PathtracingPipeline, 0, 0,
                                                                                            sizeof(pc), &pc);

        static constexpr uint32_t workgroup_width  = 16;
        static constexpr uint32_t workgroup_height = 16;

        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            (s_RendererData->PathtracedImage->GetSpecification().Width + workgroup_width - 1) / workgroup_width,
            (s_RendererData->PathtracedImage->GetSpecification().Height + workgroup_height - 1) / workgroup_height, 1);

        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
    }
    /* PATHTRACING TESTS from vk_mini_path_tracer */

    std::sort(s_RendererData->OpaqueObjects.begin(), s_RendererData->OpaqueObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::length(lhsPos - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhsPos - s_RendererData->CameraStruct.Position);

                  return lhsDist < rhsDist;  // Front to back drawing(minimize overdraw)
              });

    std::sort(s_RendererData->TransparentObjects.begin(), s_RendererData->TransparentObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::length(lhsPos - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhsPos - s_RendererData->CameraStruct.Position);

                  return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
              });

    DepthPrePass();
    DirShadowMapPass();

    HBAOPass();
    SSAOPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    LightCullingPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    GeometryPass();

    Renderer2D::Flush();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->Submit(true, false);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit(true, false);

    Application::Get().GetWindow()->CopyToWindow(s_RendererData->GBuffer->GetAttachments()[0].Attachment);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit(
        false, true, EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        {Application::Get().GetWindow()->GetSwapchain()->GetImageAvailableSemaphore()});

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();
    s_RendererData->CurrentTransferCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();
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
                                    glm::vec2(s_RendererData->CompositeFramebuffer->GetSpecification().Width,
                                              s_RendererData->CompositeFramebuffer->GetSpecification().Height)};
    // NOTE: Constants used for depth linearization from ndc depth
    {
        const float znear             = camera.GetNearPlaneDepth();
        const float zfar              = camera.GetFarPlaneDepth();
        const float depthLinearizeMul = zfar * znear / (zfar - znear);
        float depthLinearizeAdd       = zfar / (zfar - znear);

        // correct the handedness issue. need to make sure this below is correct, but I think it is.
        if (depthLinearizeMul * depthLinearizeAdd < 0) depthLinearizeAdd = -depthLinearizeAdd;
        s_RendererData->CameraStruct.DepthUnpackConsts = glm::vec2(depthLinearizeMul, depthLinearizeAdd);
    }

    s_RendererData->CullFrustum = camera.GetFrustum();

    s_RendererData->CameraUB[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraStruct, sizeof(s_RendererData->CameraStruct));
    s_BindlessRenderer->UpdateCameraData(s_RendererData->CameraUB[s_RendererData->FrameIndex]);
}

void Renderer::EndScene() {}

void Renderer::BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline>& pipeline)
{
    if (const auto pipelineToBind = s_RendererData->LastBoundPipeline.lock())
    {
        if (pipelineToBind != pipeline) commandBuffer->BindPipeline(pipeline);

        s_RendererData->LastBoundPipeline = pipeline;
    }
    else  // First call of the current frame
    {
        commandBuffer->BindPipeline(pipeline);

        s_RendererData->LastBoundPipeline = pipeline;
    }

    commandBuffer->BindShaderData(pipeline, pipeline->GetSpecification().Shader);
}

void Renderer::DrawGrid()
{
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0));

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPipeline(s_RendererData->GridPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(6);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

bool Renderer::IsInsideFrustum(const auto& renderObject)
{
    const auto& sphere = renderObject.submesh->GetBoundingSphere();

    // Adjust center by translation
    glm::vec3 scaledCenter = sphere.Center;
    {
        // extract translation part
        scaledCenter.x += renderObject.Transform[3].x;
        scaledCenter.y += renderObject.Transform[3].y;
        scaledCenter.z += renderObject.Transform[3].z;
    }

    // Adjust radius by scaling
    float scaledRadius = sphere.Radius;
    {
        // Get global scale is computed by doing the magnitude of
        // X, Y and Z model matrix's column.
        const glm::vec3 globalScale = glm::vec3(glm::length(renderObject.Transform[0]), glm::length(renderObject.Transform[1]),
                                                glm::length(renderObject.Transform[2]));

        // To wrap correctly our shape, we need the maximum scale scalar.
        const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z) + s_PFR_SMALL_NUMBER;

        // Max scale is assuming for the diameter. So, we need the half to apply it to our radius
        if (maxScale != 1.f) scaledRadius *= maxScale;
        if (maxScale > 1.f) scaledRadius *= 0.5f;
    }

    for (uint32_t i = 0; i < 6; ++i)
    {
        const auto& plane = s_RendererData->CullFrustum.Planes[i];
        if (glm::dot(plane.Normal, scaledCenter) - plane.Distance + scaledRadius <= 0.0) return false;
    }

    return true;
}

void Renderer::DepthPrePass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->DepthPrePassFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->DepthPrePassPipeline);

    auto renderMeshFunc = [&](const RenderObject& renderObject)
    {
        PushConstantBlock pc    = {renderObject.Transform};
        pc.VertexPosBufferIndex = renderObject.submesh->GetVertexPositionBuffer()->GetBindlessIndex();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex          = renderObject.submesh->GetMeshletBuffer()->GetBindlessIndex();
            pc.MeshletVerticesBufferIndex  = renderObject.submesh->GetMeshletVerticesBuffer()->GetBindlessIndex();
            pc.MeshletTrianglesBufferIndex = renderObject.submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex();

            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
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
    for (const auto& opaque : s_RendererData->OpaqueObjects)
    {
        if (IsInsideFrustum(opaque))
        {
            renderMeshFunc(opaque);
        }
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    // TODO: Should I depth pre pass transparent?? (it breaks alpha-blending)
    // s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    // for (const auto& transparent : s_RendererData->TransparentObjects)
    //{
    //    renderMeshFunc(transparent);
    //}
    // s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->DepthPrePassFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::SSAOPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->SSAO.Framebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->SSAO.Pipeline);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->SSAO.Framebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();

    // BOX-BLURRING
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->SSAOBlurFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->SSAOBlurPipeline);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->SSAOBlurFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::HBAOPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(.1f, .6f, .1f));

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
    pc.AlbedoTextureIndex = s_RendererData->FrustumDebugImage->GetBindlessIndex();
    pc.NormalTextureIndex = s_RendererData->LightHeatMapImage->GetBindlessIndex();

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->LightCullingPipeline);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->LightCullingPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);

    const auto& framebufferSpec = s_RendererData->DepthPrePassFramebuffer->GetSpecification();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        (framebufferSpec.Width + LIGHT_CULLING_TILE_SIZE - 1) / LIGHT_CULLING_TILE_SIZE,
        (framebufferSpec.Height + LIGHT_CULLING_TILE_SIZE - 1) / LIGHT_CULLING_TILE_SIZE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE, EAccessFlags::ACCESS_SHADER_READ);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::GeometryPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(__FUNCTION__, glm::vec3(0.9f, 0.5f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->GBuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    // TODO: Fix it since now I use reversed-z
    // DrawGrid();

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ForwardPlusPipeline);
    auto renderMeshFunc = [](const RenderObject& renderObject)
    {
        PushConstantBlock pc             = {renderObject.Transform};
        pc.VertexPosBufferIndex          = renderObject.submesh->GetVertexPositionBuffer()->GetBindlessIndex();
        pc.VertexAttribBufferIndex       = renderObject.submesh->GetVertexAttributeBuffer()->GetBindlessIndex();
        pc.AlbedoTextureIndex            = renderObject.submesh->GetMaterial()->GetAlbedoIndex();
        pc.NormalTextureIndex            = renderObject.submesh->GetMaterial()->GetNormalMapIndex();
        pc.MetallicRoughnessTextureIndex = renderObject.submesh->GetMaterial()->GetMetallicRoughnessIndex();
        pc.EmissiveTextureIndex          = renderObject.submesh->GetMaterial()->GetEmissiveMapIndex();
        pc.StorageImageIndex             = s_RendererData->LightStruct.DirectionalLightCount;  // LightSpace frag pos count

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardPlusPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex          = renderObject.submesh->GetMeshletBuffer()->GetBindlessIndex();
            pc.MeshletVerticesBufferIndex  = renderObject.submesh->GetMeshletVerticesBuffer()->GetBindlessIndex();
            pc.MeshletTrianglesBufferIndex = renderObject.submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex();

            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
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
    for (const auto& opaque : s_RendererData->OpaqueObjects)
    {
        if (IsInsideFrustum(opaque)) renderMeshFunc(opaque);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    for (const auto& transparent : s_RendererData->TransparentObjects)
    {
        if (IsInsideFrustum(transparent)) renderMeshFunc(transparent);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->GBuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::DirShadowMapPass()
{
    if (s_RendererData->OpaqueObjects.empty() && s_RendererData->TransparentObjects.empty()) return;

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->DirShadowMapPipeline);

    auto renderMeshFunc = [](const RenderObject& renderObject, const uint32_t directionalLightIndex)
    {
        PushConstantBlock pc = {renderObject.Transform};
        pc.StorageImageIndex = directionalLightIndex;  // Index into matrices projection array

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DirShadowMapPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        if (s_RendererSettings.bMeshShadingSupport)
        {
            pc.MeshletBufferIndex          = renderObject.submesh->GetMeshletBuffer()->GetBindlessIndex();
            pc.MeshletVerticesBufferIndex  = renderObject.submesh->GetMeshletVerticesBuffer()->GetBindlessIndex();
            pc.MeshletTrianglesBufferIndex = renderObject.submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex();

            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(meshletCount, 1, 1);

            s_RendererStats.MeshletCount += meshletCount;
        }
        else
        {
            const uint64_t offset = 0;
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindVertexBuffers(
                std::vector<Shared<Buffer>>{renderObject.submesh->GetVertexPositionBuffer()}, 0, 1, &offset);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindIndexBuffer(renderObject.submesh->GetIndexBuffer());
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawIndexed(
                renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t));
        }
        s_RendererStats.TriangleCount += renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
    };

    for (uint32_t dirLightIndex{}; dirLightIndex < s_RendererData->LightStruct.DirectionalLightCount; ++dirLightIndex)
    {
        if (!s_RendererData->LightStruct.DirectionalLights[dirLightIndex].bCastShadows) continue;

        s_RendererData->DirShadowMaps[dirLightIndex]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));
        for (const auto& opaque : s_RendererData->OpaqueObjects)
        {
            renderMeshFunc(opaque, dirLightIndex);
        }
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
        for (const auto& transparent : s_RendererData->TransparentObjects)
        {
            renderMeshFunc(transparent, dirLightIndex);
        }
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

        s_RendererData->DirShadowMaps[dirLightIndex]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    }
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

    const auto& prevDl             = s_RendererData->LightStruct.DirectionalLights[s_RendererData->LightStruct.DirectionalLightCount];
    bool bNeedsMatrixRecalculation = false;
    for (uint32_t i{}; i < 3; ++i)
    {
        if (!IsNearlyEqual(dl.Direction[i], prevDl.Direction[i]))
        {
            bNeedsMatrixRecalculation = true;
            break;
        }
    }

    s_RendererData->LightStruct.DirectionalLights[s_RendererData->LightStruct.DirectionalLightCount] = dl;

    if (dl.bCastShadows && bNeedsMatrixRecalculation)
    {
        const glm::mat4 lightView = glm::lookAt(-dl.Direction, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        const float ar = static_cast<float>(Application::Get().GetWindow()->GetSpecification().Width) /
                         static_cast<float>(Application::Get().GetWindow()->GetSpecification().Height);

        constexpr float cameraWidth = 256.f;
        constexpr float zNear       = 0.00001f;
        constexpr float zFar        = 1024.0f;
        const glm::mat4 lightProj   = glm::ortho(-cameraWidth * ar, ar * cameraWidth, -cameraWidth, cameraWidth, -cameraWidth, cameraWidth);

        s_RendererData->LightStruct.DirLightViewProjMatrices[s_RendererData->LightStruct.DirectionalLightCount] = lightProj * lightView;
    }

    ++s_RendererData->LightStruct.DirectionalLightCount;
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