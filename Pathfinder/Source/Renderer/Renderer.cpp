#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Swapchain.h"
#include "Core/Window.h"

#include "Shader.h"
#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Texture2D.h"
#include "Material.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Camera/Camera.h"
#include "Mesh.h"

#include "RayTracingBuilder.h"
#include "RenderGraph/RenderGraphBuilder.h"

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
    RayTracingBuilder::Init();

    s_RendererData->LightStruct = MakeUnique<LightsData>();

    ShaderLibrary::Load(std::vector<std::string>{"pathtrace", "Forward", "DepthPrePass", "LightCulling", "Composite", "AO/SSAO", "AO/HBAO",
                                                 "AO/AO_Blur", "Shadows/DirShadowMap", "Shadows/PointLightShadowMap",
                                                 "AtmosphericScattering", "Post/GaussianBlur"});

    std::ranges::for_each(s_RendererData->UploadHeap,
                          [](auto& uploadHeap)
                          {
                              BufferSpecification uploadHeapSpec = {EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE, 0, false, true,
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
                              ubSpec.Data                = s_RendererData->LightStruct.get();
                              ubSpec.DataSize            = sizeof(LightsData);

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
        uint32_t whiteColor                   = 0xFFFFFFFF;
        TextureSpecification whiteTextureSpec = {1, 1, &whiteColor, sizeof(whiteColor)};
        whiteTextureSpec.bBindlessUsage       = true;
        s_RendererData->WhiteTexture          = Texture2D::Create(whiteTextureSpec);
    }

    {
        FramebufferSpecification framebufferSpec = {"DepthPrePass"};

        FramebufferAttachmentSpecification depthAttachmentSpec = {EImageFormat::FORMAT_D32F,
                                                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                  ELoadOp::CLEAR,
                                                                  EStoreOp::STORE,
                                                                  glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
                                                                  false};
        depthAttachmentSpec.bBindlessUsage                     = true;

        framebufferSpec.Attachments.emplace_back(depthAttachmentSpec);
        s_RendererData->DepthPrePassFramebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                          { s_RendererData->DepthPrePassFramebuffer->Resize(width, height); });
    }

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
    depthPrePassPipelineSpec.InputBufferBindings   = {{{"inPos", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}};
    PipelineBuilder::Push(s_RendererData->DepthPrePassPipeline, depthPrePassPipelineSpec);

    {
        FramebufferSpecification framebufferSpec = {"GBuffer"};

        // Albedo
        {
            FramebufferAttachmentSpecification attachmentSpec = {EImageFormat::FORMAT_RGBA16F,
                                                                 EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 ELoadOp::LOAD,
                                                                 EStoreOp::STORE,
                                                                 glm::vec4(0),
                                                                 true,
                                                                 ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                 ESamplerFilter::SAMPLER_FILTER_LINEAR};
            framebufferSpec.Attachments.emplace_back(attachmentSpec);
        }

        // Bright color for bloom
        {
            FramebufferAttachmentSpecification attachmentSpec = {EImageFormat::FORMAT_RGBA16F,
                                                                 EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 ELoadOp::LOAD,
                                                                 EStoreOp::STORE,
                                                                 glm::vec4(0),
                                                                 true,
                                                                 ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                 ESamplerFilter::SAMPLER_FILTER_LINEAR};
            framebufferSpec.Attachments.emplace_back(attachmentSpec);
        }

#if TODO
        // View normal map
        {
            FramebufferAttachmentSpecification attachmentSpec = {EImageFormat::FORMAT_RGBA16F,
                                                                 EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 ELoadOp::LOAD,
                                                                 EStoreOp::STORE,
                                                                 glm::vec4(0),
                                                                 true,
                                                                 ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                 ESamplerFilter::SAMPLER_FILTER_LINEAR};
            attachmentSpec.bBindlessUsage                     = true;
            framebufferSpec.Attachments.emplace_back(attachmentSpec);
        }
#endif

        // Depth from depth pre pass
        {
            framebufferSpec.ExistingAttachments[2]            = s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment();
            FramebufferAttachmentSpecification attachmentSpec = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].Specification;
            attachmentSpec.LoadOp                             = ELoadOp::LOAD;
            attachmentSpec.StoreOp                            = EStoreOp::STORE;
            framebufferSpec.Attachments.emplace_back(attachmentSpec);
        }

        s_RendererData->GBuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->GBuffer->Resize(width, height); });
    }

    {
        PipelineSpecification forwardPipelineSpec = {"ForwardPlus", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        forwardPipelineSpec.Shader                = ShaderLibrary::Get("Forward");
        // Rendering view normals useless for now.
        // forwardPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        forwardPipelineSpec.bBindlessCompatible = true;
        forwardPipelineSpec.CullMode            = ECullMode::CULL_MODE_BACK;
        forwardPipelineSpec.bDepthTest          = true;
        forwardPipelineSpec.bDepthWrite         = false;
        forwardPipelineSpec.DepthCompareOp      = ECompareOp::COMPARE_OP_EQUAL;
        forwardPipelineSpec.TargetFramebuffer   = s_RendererData->GBuffer;
        forwardPipelineSpec.bMeshShading        = s_RendererSettings.bMeshShadingSupport;
        forwardPipelineSpec.bBlendEnable        = true;
        forwardPipelineSpec.BlendMode           = EBlendMode::BLEND_MODE_ALPHA;
        forwardPipelineSpec.InputBufferBindings = {{{"inPos", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}},
                                                   {
                                                       {"inColor", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4},
                                                       {"inNormal", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                                       {"inTangent", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3},
                                                       {"inUV", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC2},
                                                   }};
        PipelineBuilder::Push(s_RendererData->ForwardPlusPipeline, forwardPipelineSpec);
    }

    {
        FramebufferSpecification framebufferSpec                         = {"Composite"};
        const FramebufferAttachmentSpecification compositeAttachmentSpec = {EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32,
                                                                            EImageLayout::IMAGE_LAYOUT_UNDEFINED,
                                                                            ELoadOp::CLEAR,
                                                                            EStoreOp::STORE,
                                                                            glm::vec4(0),
                                                                            true};

        framebufferSpec.Attachments.emplace_back(compositeAttachmentSpec);
        s_RendererData->CompositeFramebuffer = Framebuffer::Create(framebufferSpec);
    }

    {
        Application::Get().GetWindow()->AddResizeCallback(
            [&](uint32_t width, uint32_t height)
            {
                s_RendererData->CompositeFramebuffer->Resize(width, height);
                s_RendererData->CompositePipeline->GetSpecification().Shader->Set("u_Albedo",
                                                                                  s_RendererData->GBuffer->GetAttachments()[0].Attachment);
            });

        PipelineSpecification compositePipelineSpec = {"Composite", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        compositePipelineSpec.TargetFramebuffer     = s_RendererData->CompositeFramebuffer;
        compositePipelineSpec.Shader                = ShaderLibrary::Get("Composite");
        compositePipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
        compositePipelineSpec.bBindlessCompatible   = true;
        PipelineBuilder::Push(s_RendererData->CompositePipeline, compositePipelineSpec);
    }

    Renderer2D::Init();

    /* vk_mini_path_tracer  */
    {
        {
            ImageSpecification imageSpec = {};
            imageSpec.bBindlessUsage     = true;
            imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
            imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;

            s_RendererData->PathtracedImage = Image::Create(imageSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                          { s_RendererData->PathtracedImage->Resize(width, height); });

        PipelineSpecification pathTracingPipelineSpec = {"Pathtracing", EPipelineType::PIPELINE_TYPE_COMPUTE};
        pathTracingPipelineSpec.Shader                = ShaderLibrary::Get("pathtrace");
        pathTracingPipelineSpec.bBindlessCompatible   = true;
        PipelineBuilder::Push(s_RendererData->PathtracingPipeline, pathTracingPipelineSpec);
    }
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
        imageSpec.bBindlessUsage     = true;
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        s_RendererData->FrustumDebugImage = Image::Create(imageSpec);
        s_RendererData->LightHeatMapImage = Image::Create(imageSpec);
    }

    {
        ImageSpecification imageSpec = {};
        imageSpec.bBindlessUsage     = true;
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        s_RendererData->CascadeDebugImage = Image::Create(imageSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            s_RendererData->FrustumDebugImage->Resize(width, height);

            s_RendererData->LightHeatMapImage->Resize(width, height);

            s_RendererData->CascadeDebugImage->Resize(width, height);

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
        FramebufferSpecification framebufferSpec                  = {"SSAO"};
        const FramebufferAttachmentSpecification aoAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                     EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                     ELoadOp::CLEAR,
                                                                     EStoreOp::STORE,
                                                                     glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                     false,
                                                                     ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                     ESamplerFilter::SAMPLER_FILTER_LINEAR};

        framebufferSpec.Attachments.emplace_back(aoAttachmentSpec);
        s_RendererData->SSAO.Framebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->SSAO.Framebuffer->Resize(width, height); });

        PipelineSpecification ssaoPipelineSpec = {"SSAO", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        ssaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO");
        // Gbuffer goes as last, so there's no way for SSAO to get it.
        // ssaoPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        ssaoPipelineSpec.TargetFramebuffer   = s_RendererData->SSAO.Framebuffer;
        ssaoPipelineSpec.CullMode            = ECullMode::CULL_MODE_BACK;
        ssaoPipelineSpec.bBindlessCompatible = true;
        PipelineBuilder::Push(s_RendererData->SSAO.Pipeline, ssaoPipelineSpec);
    }

    // Noise texture for ambient occlusions
    {
        std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
        std::mt19937_64 rndEngine;
        std::vector<glm::vec3> ssaoNoise;
        for (uint32_t i{}; i < 16; ++i)
        {
            ssaoNoise.emplace_back(randomFloats(rndEngine) * 2.0 - 1.0, randomFloats(rndEngine) * 2.0 - 1.0, 0.0f);
        }
        TextureSpecification aoNoiseTextureSpec = {4, 4, ssaoNoise.data(), ssaoNoise.size() * sizeof(ssaoNoise[0])};
        aoNoiseTextureSpec.bBindlessUsage       = true;
        aoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_LINEAR;
        aoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
        aoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA32F;
        s_RendererData->AONoiseTexture          = Texture2D::Create(aoNoiseTextureSpec);
    }

    {
        FramebufferSpecification framebufferSpec = {"HBAO"};

        const FramebufferAttachmentSpecification aoAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                     EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                     ELoadOp::CLEAR,
                                                                     EStoreOp::STORE,
                                                                     glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                     false,
                                                                     ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                     ESamplerFilter::SAMPLER_FILTER_LINEAR};

        framebufferSpec.Attachments.emplace_back(aoAttachmentSpec);
        s_RendererData->HBAO.Framebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height)
            {
                s_RendererData->HBAO.Framebuffer->Resize(width, height);

                s_RendererData->HBAO.Pipeline->GetSpecification().Shader->Set(
                    "u_DepthTex", s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment());
            });

        PipelineSpecification hbaoPipelineSpec = {"HBAO", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        hbaoPipelineSpec.TargetFramebuffer     = s_RendererData->HBAO.Framebuffer;
        hbaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/HBAO");
        hbaoPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
        hbaoPipelineSpec.bBindlessCompatible   = true;
        PipelineBuilder::Push(s_RendererData->HBAO.Pipeline, hbaoPipelineSpec);
    }

    {
        FramebufferSpecification framebufferSpec                = {"AO_Blur"};
        FramebufferAttachmentSpecification aoBlurAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                   EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                   ELoadOp::CLEAR,
                                                                   EStoreOp::STORE,
                                                                   glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                   false,
                                                                   ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                   ESamplerFilter::SAMPLER_FILTER_LINEAR};

        aoBlurAttachmentSpec.bBindlessUsage = true;
        framebufferSpec.Attachments.emplace_back(aoBlurAttachmentSpec);
        s_RendererData->BlurAO.Framebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->BlurAO.Framebuffer->Resize(width, height); });

        PipelineSpecification aoBlurPipelineSpec = {"AO_Blur", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        aoBlurPipelineSpec.TargetFramebuffer     = s_RendererData->BlurAO.Framebuffer;
        aoBlurPipelineSpec.Shader                = ShaderLibrary::Get("AO/AO_Blur");
        aoBlurPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
        aoBlurPipelineSpec.bBindlessCompatible   = true;
        PipelineBuilder::Push(s_RendererData->BlurAO.Pipeline, aoBlurPipelineSpec);
    }

    {
        // Directional Cascaded Shadow Mapping
        s_RendererData->DirShadowMaps.resize(MAX_DIR_LIGHTS);
        for (uint32_t i{}; i < s_RendererData->DirShadowMaps.size(); ++i)
        {
            std::string framebufferDebugName                     = std::string("DirShadowMap_") + std::to_string(i);
            FramebufferSpecification dirShadowMapFramebufferSpec = {framebufferDebugName.data()};

            dirShadowMapFramebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_D32F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(1.0f), false, ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_BORDER, ESamplerFilter::SAMPLER_FILTER_NEAREST, false,
                MAX_SHADOW_CASCADES - 1);

            dirShadowMapFramebufferSpec.Width = dirShadowMapFramebufferSpec.Height =
                s_ShadowsSettings.at(s_RendererData->CurrentShadowSetting);
            s_RendererData->DirShadowMaps[i] = Framebuffer::Create(dirShadowMapFramebufferSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height)
            {
                // To recalculate shadow maps view projection matrices
                s_RendererData->LightStruct = MakeUnique<LightsData>();
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
        dirShadowMapPipelineSpec.InputBufferBindings   = {{{"inPos", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}};
        PipelineBuilder::Push(s_RendererData->DirShadowMapPipeline, dirShadowMapPipelineSpec);
    }

    {
        s_RendererData->PointLightShadowMapInfos.resize(MAX_POINT_LIGHTS);

        {
            FramebufferSpecification framebufferSpec = {"PointLightShadowMap"};
            auto& pointLightDepthAttachmentSpec      = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_D32F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(1.0f), false, ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE, ESamplerFilter::SAMPLER_FILTER_NEAREST, false, 6);
            framebufferSpec.Width = framebufferSpec.Height                  = s_ShadowsSettings.at(s_RendererData->CurrentShadowSetting);
            s_RendererData->PointLightShadowMapInfos[0].PointLightShadowMap = Framebuffer::Create(framebufferSpec);
        }

        PipelineSpecification pointLightShadowMapPipelineSpec = {"PointShadowMap", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        pointLightShadowMapPipelineSpec.Shader                = ShaderLibrary::Get("Shadows/PointLightShadowMap");
        pointLightShadowMapPipelineSpec.TargetFramebuffer     = s_RendererData->PointLightShadowMapInfos[0].PointLightShadowMap;
        pointLightShadowMapPipelineSpec.bBindlessCompatible   = true;
        pointLightShadowMapPipelineSpec.bMeshShading          = s_RendererSettings.bMeshShadingSupport;
        pointLightShadowMapPipelineSpec.CullMode              = ECullMode::CULL_MODE_FRONT;
        pointLightShadowMapPipelineSpec.bDepthTest            = true;
        pointLightShadowMapPipelineSpec.bDepthWrite           = true;
        pointLightShadowMapPipelineSpec.DepthCompareOp        = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
        pointLightShadowMapPipelineSpec.InputBufferBindings   = {{{"inPos", EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3}}};
        PipelineBuilder::Push(s_RendererData->PointLightShadowMapPipeline, pointLightShadowMapPipelineSpec);
    }

    {
        PipelineSpecification asPipelineSpec = {"AtmosphericScattering", EPipelineType::PIPELINE_TYPE_GRAPHICS};
        asPipelineSpec.Shader                = ShaderLibrary::Get("AtmosphericScattering");
        asPipelineSpec.TargetFramebuffer     = s_RendererData->GBuffer;
        asPipelineSpec.bBindlessCompatible   = true;
        asPipelineSpec.CullMode              = ECullMode::CULL_MODE_BACK;
        PipelineBuilder::Push(s_RendererData->AtmospherePipeline, asPipelineSpec);
    }

    {
        // Ping-pong framebuffers
        for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
        {
            const std::string blurTypeStr            = (i == 0) ? "Horiz" : "Vert";
            FramebufferSpecification framebufferSpec = {"BloomFramebuffer" + blurTypeStr};
            auto& bloomAttachmentSpec                = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_RGBA16F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(0.0f), false, ESamplerWrap::SAMPLER_WRAP_REPEAT, ESamplerFilter::SAMPLER_FILTER_LINEAR);
            s_RendererData->BloomFramebuffer[i] = Framebuffer::Create(framebufferSpec);

            PipelineSpecification bloomPipelineSpec = {"Bloom" + blurTypeStr, EPipelineType::PIPELINE_TYPE_GRAPHICS};
            bloomPipelineSpec.Shader                = ShaderLibrary::Get("Post/GaussianBlur");
            bloomPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(i);
            bloomPipelineSpec.TargetFramebuffer   = s_RendererData->BloomFramebuffer[i];
            bloomPipelineSpec.bBindlessCompatible = true;
            bloomPipelineSpec.CullMode            = ECullMode::CULL_MODE_BACK;
            PipelineBuilder::Push(s_RendererData->BloomPipeline[i], bloomPipelineSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height)
            {
                std::vector<Shared<Image>> attachments;
                for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
                {
                    s_RendererData->BloomFramebuffer[i]->Resize(width, height);

                    attachments.emplace_back(i == 0 ? s_RendererData->GBuffer->GetAttachments()[1].Attachment
                                                    : s_RendererData->BloomFramebuffer[i - 1]->GetAttachments()[0].Attachment);
                }

                // These 2 reference the same shader, but use 1 descriptor set.. duhh.
                for (auto& bloomPipeline : s_RendererData->BloomPipeline)
                    bloomPipeline->GetSpecification().Shader->Set("u_Image", attachments);

                s_RendererData->CompositePipeline->GetSpecification().Shader->Set(
                    "u_BloomBlur", s_RendererData->BloomFramebuffer[1]->GetAttachments()[0].Attachment);
            });
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

    s_RendererData->CompositePipeline->GetSpecification().Shader->Set("u_Albedo", s_RendererData->GBuffer->GetAttachments()[0].Attachment);
    s_RendererData->CompositePipeline->GetSpecification().Shader->Set("u_BloomBlur",
                                                                      s_RendererData->BloomFramebuffer[1]->GetAttachments()[0].Attachment);

    s_RendererData->HBAO.Pipeline->GetSpecification().Shader->Set("u_DepthTex",
                                                                  s_RendererData->DepthPrePassFramebuffer->GetDepthAttachment());
    s_RendererData->HBAO.Pipeline->GetSpecification().Shader->Set("u_NoiseTex", s_RendererData->AONoiseTexture);

    {
        std::vector<Shared<Image>> attachments(s_RendererData->DirShadowMaps.size());
        for (size_t i{}; i < attachments.size(); ++i)
            attachments[i] = s_RendererData->DirShadowMaps[i]->GetDepthAttachment();

        s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set("u_DirShadowmap", attachments);
    }

    {
        std::vector<Shared<Image>> attachments;
        for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
        {
            attachments.emplace_back(i == 0 ? s_RendererData->GBuffer->GetAttachments()[1].Attachment
                                            : s_RendererData->BloomFramebuffer[i - 1]->GetAttachments()[0].Attachment);
        }

        // These 2 reference the same shader, but use 1 descriptor set.. duhh.
        for (auto& bloomPipeline : s_RendererData->BloomPipeline)
            bloomPipeline->GetSpecification().Shader->Set("u_Image", attachments);
    }

    s_RendererData->RenderGraph = RenderGraphBuilder::Create("Assets/RenderGraphDescription.pfr");

    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D created!");
}

void Renderer::Shutdown()
{
    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();
    PipelineBuilder::Shutdown();
    RayTracingBuilder::Shutdown();

    s_RendererData.reset();
    s_BindlessRenderer.reset();

    SamplerStorage::Shutdown();
    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D destroyed!");
}

void Renderer::Begin()
{
    Timer t = {};
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

    s_RendererData->CameraStruct                       = {};
    s_RendererData->LightStruct->DirectionalLightCount = 0;
    s_RendererData->LightStruct->SpotLightCount        = 0;
    s_RendererData->LightStruct->PointLightCount       = 0;

    {
        const auto& renderTimestamps = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetTimestampsResults();
        s_RendererStats.GPUTime      = std::accumulate(renderTimestamps.begin(), renderTimestamps.end(), s_RendererStats.GPUTime);

#define LOG_RENDERER 0

#if LOG_RENDERER
        const auto& renderPipelineStats  = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();
        const auto& computePipelineStats = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();

        LOG_TAG_INFO(RENDERER, "RENDERER STATISTICS: ");
        LOG_TAG_INFO(SAMPLER_STORAGE, "Sampler count: %u", SamplerStorage::GetSamplerCount());
        LOG_TAG_INFO(RENDERER, "DepthPre-Pass: %0.4f (ms)", renderTimestamps[0]);
        LOG_TAG_INFO(RENDERER, "DirShadowMap-Pass: %0.4f (ms)", renderTimestamps[1]);
        LOG_TAG_INFO(RENDERER, "SSAO-Pass: %0.4f (ms)", renderTimestamps[2]);
        LOG_TAG_INFO(RENDERER, "SSAOBlur-Pass: %0.4f (ms)", renderTimestamps[3]);
        LOG_TAG_INFO(RENDERER, "LightCulling-Pass: %0.4f (ms)", renderTimestamps[4]);
        LOG_TAG_INFO(RENDERER, "Geometry-Pass: %0.4f (ms)", renderTimestamps[5]);
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

    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
}

void Renderer::Flush()
{
    Timer t = {};
    s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->SetData(s_RendererData->LightStruct.get(), sizeof(LightsData));
    s_BindlessRenderer->UpdateLightData(s_RendererData->LightsSSBO[s_RendererData->FrameIndex]);

    s_BindlessRenderer->UpdateDataIfNeeded();

    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    /* PATHTRACING TESTS from vk_mini_path_tracer */
    if (s_RendererSettings.bRTXSupport)
    {
        s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel(
            s_RendererData->PathtracingPipeline->GetSpecification().DebugName);

        // TESTING
        PushConstantBlock pc    = {};
        pc.StorageImageIndex    = s_RendererData->PathtracedImage->GetBindlessIndex();
        pc.VertexPosBufferIndex = s_RendererData->OpaqueObjects[0].submesh->GetVertexPositionBuffer()->GetBindlessIndex();
        pc.MeshIndexBufferIndex = s_RendererData->OpaqueObjects[0].submesh->GetIndexBuffer()->GetBindlessIndex();

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

    s_RendererData->GBuffer->Clear(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    DepthPrePass();
    DirShadowMapPass();
    PointLightShadowMapPass();

    AtmosphericScatteringPass();

    SSAOPass();
    HBAOPass();
    s_RendererData->BlurAO.Pipeline->GetSpecification().Shader->Set("u_AO",
                                                                    s_RendererData->SSAO.Framebuffer->GetAttachments()[0].Attachment);
    BlurAOPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    LightCullingPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    GeometryPass();

    Renderer2D::Flush();

    BloomPass();
    CompositePass();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    void* tts       = s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->GetTimelineSemaphore();
    uint64_t ttsVal = s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->GetTimelineValue();
    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->Submit(false, false, ttsVal);

    void* cts       = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->GetTimelineSemaphore();
    uint64_t ctsVal = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->GetTimelineValue();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit(
        false, false, ctsVal, {EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT}, {tts}, {ttsVal});

    Application::Get().GetWindow()->CopyToWindow(s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit(
        false, true, UINT64_MAX,
        {EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT},
        {Application::Get().GetWindow()->GetSwapchain()->GetImageAvailableSemaphore(), cts, tts}, {1, ctsVal, ttsVal});

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();
    s_RendererData->CurrentTransferCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
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
                                    camera.GetZoom(),
                                    glm::vec2(s_RendererData->CompositeFramebuffer->GetSpecification().Width,
                                              s_RendererData->CompositeFramebuffer->GetSpecification().Height)};

    s_RendererData->CullFrustum = camera.GetFrustum();

    s_RendererData->LightStruct->CascadePlaneDistances[0] = camera.GetFarPlaneDepth() / 400.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[1] = camera.GetFarPlaneDepth() / 200.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[2] = camera.GetFarPlaneDepth() / 80.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[3] = camera.GetFarPlaneDepth() / 15.0f;

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

bool Renderer::IsInsideFrustum(const auto& renderObject)
{
    const auto& sphere           = renderObject.submesh->GetBoundingSphere();
    const glm::vec3 scaledCenter = glm::vec3(renderObject.Transform * vec4(sphere.Center, 1));

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
        scaledRadius *= maxScale;
        if (!IsNearlyEqual(maxScale, 1.0f))
        {
            // NOTE: I constructed BS via radius, not diameter, so I do mul *0.5 only if scale changes, since by default it's real radius.
            scaledRadius *= 0.5f;
        }
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
    if (IsWorldEmpty()) return;

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

    /*
    // TODO: Should I depth pre pass transparent?? (it breaks alpha-blending)
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    for (const auto& transparent : s_RendererData->TransparentObjects)
    {
        renderMeshFunc(transparent);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
    */

    s_RendererData->DepthPrePassFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::SSAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->SSAO.Framebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    PushConstantBlock pc  = {};
    pc.StorageImageIndex  = s_RendererData->AONoiseTexture->GetBindlessIndex();
    pc.AlbedoTextureIndex = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;

#if TODO
    pc.MeshIndexBufferIndex = s_RendererData->GBuffer->GetAttachments()[2].m_Index;
#endif

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->SSAO.Pipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->SSAO.Pipeline, 0, 0, sizeof(pc),
                                                                                       &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->SSAO.Framebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::HBAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->HBAO.Framebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->HBAO.Pipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->HBAO.Framebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BlurAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->BlurAO.Framebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->BlurAO.Pipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->BlurAO.Framebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::LightCullingPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("LightCullingPass", glm::vec3(0.9f, 0.9f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    PushConstantBlock pc  = {};
    pc.AlbedoTextureIndex = s_RendererData->FrustumDebugImage->GetBindlessIndex();
    pc.StorageImageIndex  = s_RendererData->LightHeatMapImage->GetBindlessIndex();

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

void Renderer::AtmosphericScatteringPass()
{
    if (IsWorldEmpty() || s_RendererData->LightStruct->DirectionalLightCount == 0) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->GBuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->AtmospherePipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->GBuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::GeometryPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->GBuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ForwardPlusPipeline);
    auto renderMeshFunc = [](const RenderObject& renderObject)
    {
        PushConstantBlock pc       = {renderObject.Transform};
        pc.VertexPosBufferIndex    = renderObject.submesh->GetVertexPositionBuffer()->GetBindlessIndex();
        pc.VertexAttribBufferIndex = renderObject.submesh->GetVertexAttributeBuffer()->GetBindlessIndex();
        pc.MaterialBufferIndex     = renderObject.submesh->GetMaterial()->GetBufferIndex();
        pc.pad0.x                  = 25.0f;
        pc.AlbedoTextureIndex      = s_RendererData->CascadeDebugImage->GetBindlessIndex();
        pc.StorageImageIndex       = s_RendererData->BlurAO.Framebuffer->GetAttachments()[0].m_Index;

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
}

void Renderer::DirShadowMapPass()
{
    if (IsWorldEmpty() || s_RendererData->LightStruct->DirectionalLightCount == 0) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

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

    for (uint32_t dirLightIndex{}; dirLightIndex < s_RendererData->LightStruct->DirectionalLightCount; ++dirLightIndex)
    {
        if (!s_RendererData->LightStruct->DirectionalLights[dirLightIndex].bCastShadows) continue;

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->DirShadowMapPipeline);
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

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::PointLightShadowMapPass()
{
    if (IsWorldEmpty() || s_RendererData->LightStruct->PointLightCount == 0) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    auto renderMeshFunc = [](const RenderObject& renderObject, const uint32_t pointLightIndex)
    {
        PushConstantBlock pc = {renderObject.Transform};
        pc.StorageImageIndex = pointLightIndex;  // Index into matrices projection array
        pc.pad0.x            = 25.0f;

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->PointLightShadowMapPipeline, 0,
                                                                                           0, sizeof(pc), &pc);

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

    for (const auto& pointLightShadowMapInfo : s_RendererData->PointLightShadowMapInfos)
    {
        if (pointLightShadowMapInfo.LightAndMatrixIndex == INVALID_LIGHT_INDEX) continue;

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->PointLightShadowMapPipeline);
        pointLightShadowMapInfo.PointLightShadowMap->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));
        for (const auto& opaque : s_RendererData->OpaqueObjects)
        {
            renderMeshFunc(opaque, pointLightShadowMapInfo.LightAndMatrixIndex);
        }
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
        for (const auto& transparent : s_RendererData->TransparentObjects)
        {
            renderMeshFunc(transparent, pointLightShadowMapInfo.LightAndMatrixIndex);
        }
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

        pointLightShadowMapInfo.PointLightShadowMap->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BloomPass()
{
    if (IsWorldEmpty()) return;
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
    {
        s_RendererData->BloomFramebuffer[i]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->BloomPipeline[i]);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

        s_RendererData->BloomFramebuffer[i]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::CompositePass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->CompositeFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->CompositePipeline);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->CompositeFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
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
    if (s_RendererData->LightStruct->DirectionalLightCount + 1 > MAX_DIR_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max directional light limit reached!");
        return;
    }

    const auto& prevDl             = s_RendererData->LightStruct->DirectionalLights[s_RendererData->LightStruct->DirectionalLightCount];
    bool bNeedsMatrixRecalculation = false;
    for (uint32_t i{}; i < 3; ++i)
    {
        if (!IsNearlyEqual(dl.Direction[i], prevDl.Direction[i]))
        {
            bNeedsMatrixRecalculation = true;
            break;
        }
    }

    // TODO: Maybe remove if and calculate every frame? CHECK IT
    s_RendererData->LightStruct->DirectionalLights[s_RendererData->LightStruct->DirectionalLightCount] = dl;
    if (dl.bCastShadows && bNeedsMatrixRecalculation)
    {
        const auto GetFrustumCornersInWorldSpace = [](const glm::mat4& proj, const glm::mat4& view) -> std::vector<glm::vec4>
        {
            const glm::mat4 invProj = glm::inverse(proj * view);
            std::vector<glm::vec4> frustumCorners;
            for (int32_t x = -1; x < 1; ++x)
            {
                for (int32_t y = -1; y < 1; ++y)
                {
                    for (int32_t z = 0; z < 1; ++z)  // NDC for Z in vulkan is [0, 1]
                    {
                        const glm::vec4 pt = invProj * glm::vec4(x, y, z, 1);
                        frustumCorners.emplace_back(pt / pt.w);
                    }
                }
            }

            return frustumCorners;
        };

        const float ar = static_cast<float>(Application::Get().GetWindow()->GetSpecification().Width) /
                         static_cast<float>(Application::Get().GetWindow()->GetSpecification().Height);
        for (uint32_t cascadeIndex = 0; cascadeIndex < MAX_SHADOW_CASCADES; ++cascadeIndex)
        {
            float zNear = 0.0f, zFar = 0.0f;
            if (cascadeIndex == 0)
            {
                zNear = s_RendererData->CameraStruct.zNear;
                zFar  = s_RendererData->LightStruct->CascadePlaneDistances[cascadeIndex];
            }
            else if (cascadeIndex < MAX_SHADOW_CASCADES)
            {
                zNear = s_RendererData->LightStruct->CascadePlaneDistances[cascadeIndex - 1];
                zFar  = s_RendererData->LightStruct->CascadePlaneDistances[cascadeIndex];
            }
            else
            {
                zNear = s_RendererData->LightStruct->CascadePlaneDistances[cascadeIndex];
                zFar  = s_RendererData->CameraStruct.zFar;
            }

            const glm::mat4 proj = glm::perspective(glm::radians(s_RendererData->CameraStruct.FOV), ar, zNear, zFar);

            const auto corners = GetFrustumCornersInWorldSpace(proj, s_RendererData->CameraStruct.View);
            glm::vec3 center(0.f);
            for (const auto& v : corners)
            {
                center += glm::vec3(v);
            }
            center /= corners.size();

            const auto lightView = glm::lookAt(center + dl.Direction, center, glm::vec3(0.0f, 1.0f, 0.0f));

            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();
            for (const auto& v : corners)
            {
                const auto trf = lightView * v;
                minX           = std::min(minX, trf.x);
                maxX           = std::max(maxX, trf.x);
                minY           = std::min(minY, trf.y);
                maxY           = std::max(maxY, trf.y);
                minZ           = std::min(minZ, trf.z);
                maxZ           = std::max(maxZ, trf.z);
            }

            // Tune this parameter according to the scene
            constexpr float zMult = 10.0f;
            if (minZ < 0)
                minZ *= zMult;
            else
                minZ /= zMult;

            if (maxZ < 0)
                maxZ /= zMult;
            else
                maxZ *= zMult;

            const glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

            s_RendererData->LightStruct
                ->DirLightViewProjMatrices[s_RendererData->LightStruct->DirectionalLightCount * MAX_SHADOW_CASCADES + cascadeIndex] =
                lightProj * lightView;
        }
    }

    ++s_RendererData->LightStruct->DirectionalLightCount;
}

void Renderer::AddPointLight(const PointLight& pl)
{
    if (s_RendererData->LightStruct->PointLightCount + 1 > MAX_POINT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max point light limit reached!");
        return;
    }

    const auto& prevPl             = s_RendererData->LightStruct->PointLights[s_RendererData->LightStruct->PointLightCount];
    bool bNeedsMatrixRecalculation = false;
    for (uint32_t i{}; i < 3; ++i)
    {
        if (!IsNearlyEqual(pl.Position[i], prevPl.Position[i]))
        {
            bNeedsMatrixRecalculation = true;
            break;
        }
    }

    s_RendererData->LightStruct->PointLights[s_RendererData->LightStruct->PointLightCount] = pl;
    if (pl.bCastShadows && bNeedsMatrixRecalculation)
    {
        auto& currentPointLightShadowMapInfo = s_RendererData->PointLightShadowMapInfos[s_RendererData->LightStruct->PointLightCount];
        if (!currentPointLightShadowMapInfo.PointLightShadowMap)
        {
            FramebufferSpecification framebufferSpec = {"PointLightShadowMap"};
            auto& pointLightDepthAttachmentSpec      = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_D32F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(1.0f), false, ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE, ESamplerFilter::SAMPLER_FILTER_NEAREST, 6);

            framebufferSpec.Width = framebufferSpec.Height     = s_ShadowsSettings.at(s_RendererData->CurrentShadowSetting);
            currentPointLightShadowMapInfo.PointLightShadowMap = Framebuffer::Create(framebufferSpec);
        }
        currentPointLightShadowMapInfo.LightAndMatrixIndex = s_RendererData->LightStruct->PointLightCount;

        const float ar = static_cast<float>(Application::Get().GetWindow()->GetSpecification().Width) /
                         static_cast<float>(Application::Get().GetWindow()->GetSpecification().Height);
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), ar, 1.0f, 25.0f);

        std::vector<glm::mat4> shadowTransforms;  // right, left, top, bottom, near, far
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
        shadowTransforms.emplace_back(proj * glm::lookAt(pl.Position, pl.Position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));

        for (uint32_t i = 0; i < 6; ++i)
            s_RendererData->LightStruct->PointLightViewProjMatrices[currentPointLightShadowMapInfo.LightAndMatrixIndex * 6 + i] =
                shadowTransforms[i];

        s_RendererData->ForwardPlusPipeline->GetSpecification().Shader->Set(
            "u_PointShadowmap", std::vector<Shared<Image>>{currentPointLightShadowMapInfo.PointLightShadowMap->GetDepthAttachment()});
    }

    ++s_RendererData->LightStruct->PointLightCount;
}

void Renderer::AddSpotLight(const SpotLight& sl)
{
    if (s_RendererData->LightStruct->SpotLightCount + 1 > MAX_SPOT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max spot light limit reached!");
        return;
    }

    s_RendererData->LightStruct->SpotLights[s_RendererData->LightStruct->SpotLightCount++] = sl;
}

Shared<Image> Renderer::GetFinalPassImage()
{
    PFR_ASSERT(s_RendererData->CompositeFramebuffer && s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment,
               "Invalid composite framebuffer or its attachment!");
    return s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment;
}

}  // namespace Pathfinder