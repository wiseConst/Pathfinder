#include <PathfinderPCH.h>
#include "Renderer.h"
#include "Renderer2D.h"

#include <Core/Application.h>
#include "Swapchain.h"
#include <Core/Window.h>
#include "GraphicsContext.h"

#include "Shader.h"
#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Texture.h"
#include "Material.h"
#include "Buffer.h"
#include "Camera/Camera.h"
#include "Mesh/Mesh.h"
#include "Mesh/Submesh.h"

#include "HWRT.h"
#include "Debug/DebugRenderer.h"

#include "RenderGraph/RenderGraph.h"

// Okay, main issue for now is that cpu->gpu uploading doesn't work without ImmediateSubmit since I don't offset memory...(i copy data to
// the same location for staging heap) Possible solution is to create lots of staging buffers for frame OR upload & wait rn Hint: Create
// GrowableHeap, with offset each time I memcpy anythin.

namespace Pathfinder
{

void Renderer::Init()
{
    s_RendererData              = MakeUnique<RendererData>();
    s_RendererSettings.bVSync   = Application::Get().GetWindow()->IsVSync();
    s_RendererData->LightStruct = MakeUnique<LightData>();

    std::ranges::for_each(s_RendererData->UploadHeap,
                          [](auto& uploadHeap)
                          {
                              const BufferSpecification uploadHeapSpec = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED,
                                                                          .UsageFlags = EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE,
                                                                          .Capacity   = s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY};

                              uploadHeap = Buffer::Create(uploadHeapSpec);
                          });

    s_DescriptorManager = DescriptorManager::Create();
    SamplerStorage::Init();
    TextureManager::Init();
    ShaderLibrary::Init();
    PipelineLibrary::Init();
    RayTracingBuilder::Init();

    ShaderLibrary::Load({{"DepthPrePass"},
                         {"SSShadows"},
                         {"ObjectCulling"},
                         {"ForwardPlus"},
                         {"LightCulling", {{"ADVANCED_CULLING", ""}}},
                         {"ComputeFrustums"},
                         {"Composite"},
                         {"AO/SSAO"},
                         {"Post/GaussianBlur"},
                         {"Post/MedianBlur"},
                         {"Post/BoxBlur"},
                         /*{"RayTrace"}*/});

    for (uint8_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
    {
        const CommandBufferSpecification cbSpec = {.Type       = ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL,
                                                   .Level      = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                                                   .FrameIndex = frameIndex,
                                                   .ThreadID   = ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};

        s_RendererData->RenderCommandBuffer.at(frameIndex) = CommandBuffer::Create(cbSpec);
    }

    ShaderLibrary::WaitUntilShadersLoaded();
    CreatePipelines();

    Renderer2D::Init();
#if PFR_DEBUG
    DebugRenderer::Init();
#endif

    Application::Get().GetWindow()->AddResizeCallback(
        [](const WindowResizeData& resizeData)
        {
            s_RendererData->DepthPrePass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->LightCullingPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->SSSPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->SSAOPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->AOBlurPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->GBufferPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->BloomPass.OnResize(resizeData.Width, resizeData.Height);
            s_RendererData->FinalCompositePass.OnResize(resizeData.Width, resizeData.Height);
        });

    // Noise texture for ambient occlusions
    {
        const auto& appSpec       = Application::Get().GetSpecification();
        const auto rotTexturePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / "Other/rot_texture.bmp";
        int32_t x = 0, y = 0, channels = 0;
        uint8_t* rawImageData = reinterpret_cast<uint8_t*>(ImageUtils::LoadRawImage(rotTexturePath, false, &x, &y, &channels));
        void* rgbaImageData   = ImageUtils::ConvertRgbToRgba(rawImageData, x, y);

        const TextureSpecification aoNoiseTS{
            .DebugName = "Default_NoiseAO",
            .Width     = static_cast<uint32_t>(x),
            .Height    = static_cast<uint32_t>(y),
            .Wrap      = ESamplerWrap::SAMPLER_WRAP_REPEAT,
            .Filter    = ESamplerFilter::SAMPLER_FILTER_NEAREST,
            .Format    = EImageFormat::FORMAT_RGBA32F,
        };
        s_RendererData->AONoiseTexture = Texture::Create(aoNoiseTS, rgbaImageData, x * y * sizeof(glm::vec4));

        free(rgbaImageData);
        ImageUtils::UnloadRawImage(rawImageData);
    }

    PipelineLibrary::Compile();
    LOG_TRACE("{}", __FUNCTION__);

    const auto& windowSpec = Application::Get().GetWindow()->GetSpecification();

    s_RendererData->FramePreparePass   = {};
    s_RendererData->ObjectCullingPass  = {};
    s_RendererData->DepthPrePass       = DepthPrePass(windowSpec.Width, windowSpec.Height);
    s_RendererData->LightCullingPass   = LightCullingPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->SSSPass            = ScreenSpaceShadowsPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->SSAOPass           = SSAOPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->AOBlurPass         = AOBlurPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->GBufferPass        = GBufferPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->BloomPass          = BloomPass(windowSpec.Width, windowSpec.Height);
    s_RendererData->FinalCompositePass = FinalCompositePass(windowSpec.Width, windowSpec.Height);
}

void Renderer::Shutdown()
{
#if PFR_DEBUG
    DebugRenderer::Shutdown();
#endif

    Renderer2D::Shutdown();
    ShaderLibrary::Shutdown();
    PipelineLibrary::Shutdown();
    RayTracingBuilder::Shutdown();
    TextureManager::Shutdown();

    s_RendererData.reset();
    s_DescriptorManager.reset();

    SamplerStorage::Shutdown();

    LOG_TRACE("{}", __FUNCTION__);
}

void Renderer::Begin()
{
    s_RendererData->bIsFrameBegin = true;
    ShaderLibrary::DestroyGarbageIfNeeded();
    TextureManager::LinkLoadedTexturesWithMeshes();

    // Update VSync state.
    auto& window = Application::Get().GetWindow();
    window->SetVSync(s_RendererSettings.bVSync);

    s_RendererData->CameraStruct.FullResolution    = glm::vec2(window->GetSpecification().Width, window->GetSpecification().Height);
    s_RendererData->CameraStruct.InvFullResolution = 1.f / s_RendererData->CameraStruct.FullResolution;

    s_RendererData->bAnybodyCastsShadows = false;
    s_RendererData->LastBoundPipeline.reset();
    s_RendererData->PassStats.clear();
    s_RendererData->PipelineStats.clear();
    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    s_RendererData->FrameIndex = window->GetCurrentFrameIndex();

    s_RendererData->UploadHeap.at(s_RendererData->FrameIndex)->Resize(s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY);

    uint32_t prevPoolCount              = s_RendererStats.DescriptorPoolCount;
    uint32_t prevDescriptorSetCount     = s_RendererStats.DescriptorSetCount;
    uint32_t prevImageViewCount         = s_RendererStats.ImageViewCount;
    s_RendererStats                     = {};
    s_RendererStats.DescriptorPoolCount = prevPoolCount;
    s_RendererStats.DescriptorSetCount  = prevDescriptorSetCount;
    s_RendererStats.ImageViewCount      = prevImageViewCount;

    s_RendererData->LightStruct->PointLightCount           = s_RendererData->LightStruct->SpotLightCount =
        s_RendererData->LightStruct->DirectionalLightCount = 0;

    s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex)->BeginRecording(true);

    // NOTE: Once per frame
    // Bind mega descriptor set to render command buffer for Graphics, Compute, RT stages.
    s_DescriptorManager->Bind(s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex));
    s_DescriptorManager->Bind(s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex),
                              EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    s_DescriptorManager->Bind(s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex),
                              EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);

    Renderer2D::Begin();

    GraphicsContext::Get().FillMemoryBudgetStats(s_RendererStats.MemoryBudgets);
}

void Renderer::Flush(const Unique<UILayer>& uiLayer)
{
    auto rg = MakeUnique<RenderGraph>(s_RendererData->FrameIndex, std::string(s_ENGINE_NAME), s_RendererData->ResourcePool);

    s_RendererData->FramePreparePass.AddPass(rg);   // Set camera data, light data, etc..
    s_RendererData->ObjectCullingPass.AddPass(rg);  // Cull objects in compute, fill indirect arg buffers.
    s_RendererData->DepthPrePass.AddPass(rg);
    s_RendererData->LightCullingPass.AddPass(rg);  // Cull lights, fill buffers with culled indices.
    s_RendererData->SSSPass.AddPass(rg);           // ScreenSpace shadows
    s_RendererData->SSAOPass.AddPass(rg);          // ssao-depth-reconstruction
    s_RendererData->AOBlurPass.AddPass(rg);        // box-blur-ssao
    s_RendererData->GBufferPass.AddPass(rg);       // Forward+ opaque, transparent
    // TODO: Renderer2D Pass
    s_RendererData->BloomPass.AddPass(rg);  // Horizontal+Vertical non pbr bloom
    // TODO: DebugRenderer Pass
    s_RendererData->FinalCompositePass.AddPass(rg);  // Assemble bloom, albedo and blit into swapchain.

    rg->Build();
    rg->Execute();

    if (uiLayer) uiLayer->EndRender();

    s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex)->EndRecording();

    const auto& swapchain = Application::Get().GetWindow()->GetSwapchain();
    const auto swapchainImageAvailableSyncPoint =
        SyncPoint::Create(swapchain->GetImageAvailableSemaphore(), 1, EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    const auto swapchainRenderFinishedSyncPoint =
        SyncPoint::Create(swapchain->GetRenderSemaphore(), 1, EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    s_RendererData->RenderCommandBuffer.at(s_RendererData->FrameIndex)
        ->Submit({swapchainImageAvailableSyncPoint}, {swapchainRenderFinishedSyncPoint}, swapchain->GetRenderFence());
    s_RendererData->bIsFrameBegin = false;
}

void Renderer::BeginScene(const Camera& camera)
{
    if (s_RendererData->CameraStruct.FOV != camera.GetZoom()) s_RendererData->LightCullingPass.SetRecomputeLightCullFrustums(true);
    s_RendererData->CameraStruct.ViewFrustum       = camera.GetFrustum();
    s_RendererData->CameraStruct.View              = camera.GetView();
    s_RendererData->CameraStruct.Projection        = camera.GetProjection();
    s_RendererData->CameraStruct.ViewProjection    = camera.GetViewProjection();
    s_RendererData->CameraStruct.InverseProjection = camera.GetInverseProjection();
    s_RendererData->CameraStruct.Position          = camera.GetPosition();
    s_RendererData->CameraStruct.zNear             = camera.GetNearPlaneDepth();
    s_RendererData->CameraStruct.zFar              = camera.GetFarPlaneDepth();
    s_RendererData->CameraStruct.FOV               = camera.GetZoom();
}

void Renderer::EndScene() {}

void Renderer::BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline> pipeline)
{
    if (const auto pipelineToBind = s_RendererData->LastBoundPipeline.lock())
    {
        if (pipelineToBind != pipeline) commandBuffer->BindPipeline(pipeline);

        s_RendererData->LastBoundPipeline = pipeline;
        return;
    }

    commandBuffer->BindPipeline(pipeline);
    s_RendererData->LastBoundPipeline = pipeline;
}

void Renderer::SubmitMesh(const Shared<Mesh>& mesh, const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation)
{
    for (auto& submesh : mesh->GetSubmeshes())
    {
        if (submesh->GetMaterial()->IsOpaque())
            s_RendererData->OpaqueObjects.emplace_back(submesh, translation, scale, orientation);
        else
            s_RendererData->TransparentObjects.emplace_back(submesh, translation, scale, orientation);
    }
}

void Renderer::AddDirectionalLight(const DirectionalLight& dl)
{
    if (s_RendererData->LightStruct->DirectionalLightCount >= MAX_DIR_LIGHTS)
    {
        LOG_WARN("Max directional light limit reached!");
        return;
    }

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || dl.bCastShadows;
    s_RendererData->LightStruct->DirectionalLights[s_RendererData->LightStruct->DirectionalLightCount++] = dl;
}

void Renderer::AddPointLight(const PointLight& pl)
{
    if (s_RendererData->LightStruct->PointLightCount >= MAX_POINT_LIGHTS)
    {
        LOG_WARN("Max point light limit reached!");
        return;
    }

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || pl.bCastShadows;
    s_RendererData->LightStruct->PointLights[s_RendererData->LightStruct->PointLightCount++] = pl;
}

void Renderer::AddSpotLight(const SpotLight& sl)
{
    if (s_RendererData->LightStruct->SpotLightCount >= MAX_SPOT_LIGHTS)
    {
        LOG_WARN("Max spot light limit reached!");
        return;
    }

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || sl.bCastShadows;
    s_RendererData->LightStruct->SpotLights[s_RendererData->LightStruct->SpotLightCount++] = sl;
}

Shared<Image> Renderer::GetFinalPassImage()
{
    PFR_ASSERT(false, "NOT IMPLEMENTED!");
    PFR_ASSERT(s_RendererData, "RendererData is not valid!");
    return nullptr;  // s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment;
}

void Renderer::CreatePipelines()
{
    // Compute Light-Culling Frustums && Light-Culling
    {
        PipelineSpecification computeFrustumsPS     = {.DebugName       = "ComputeFrustums",
                                                       .PipelineOptions = MakeOptional<ComputePipelineOptions>(),
                                                       .Shader          = ShaderLibrary::Get("ComputeFrustums"),
                                                       .PipelineType    = EPipelineType::PIPELINE_TYPE_COMPUTE};
        s_RendererData->ComputeFrustumsPipelineHash = PipelineLibrary::Push(computeFrustumsPS);

        PipelineSpecification lightCullingPS     = {.DebugName       = "LightCulling",
                                                    .PipelineOptions = MakeOptional<ComputePipelineOptions>(),
                                                    .Shader          = ShaderLibrary::Get({"LightCulling", {{"ADVANCED_CULLING", ""}}}),
                                                    .PipelineType    = EPipelineType::PIPELINE_TYPE_COMPUTE};
        s_RendererData->LightCullingPipelineHash = PipelineLibrary::Push(lightCullingPS);
    }

    // Object-Culling
    {
        PipelineSpecification objectCullingPS     = {.DebugName       = "ObjectCulling",
                                                     .PipelineOptions = MakeOptional<ComputePipelineOptions>(),
                                                     .Shader          = ShaderLibrary::Get("ObjectCulling"),
                                                     .PipelineType    = EPipelineType::PIPELINE_TYPE_COMPUTE};
        s_RendererData->ObjectCullingPipelineHash = PipelineLibrary::Push(objectCullingPS);
    }

    constexpr auto reversedDepthCompareOp    = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    constexpr auto depthPrePassTextureFormat = EImageFormat::FORMAT_D32F;

    // Depth Pre-Pass
    {
        const GraphicsPipelineOptions depthGPO = {.Formats        = {depthPrePassTextureFormat},
                                                  .CullMode       = ECullMode::CULL_MODE_BACK,
                                                  .bMeshShading   = true,
                                                  .bDepthTest     = true,
                                                  .bDepthWrite    = true,
                                                  .DepthCompareOp = reversedDepthCompareOp};

        PipelineSpecification depthPrePassPS = {.DebugName       = "DepthPrePass",
                                                .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(depthGPO),
                                                .Shader          = ShaderLibrary::Get("DepthPrePass"),
                                                .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};

        s_RendererData->DepthPrePassPipelineHash = PipelineLibrary::Push(depthPrePassPS);
    }

    // Forward+
    {
        GraphicsPipelineOptions forwardPlusGPO = {
            .Formats        = {EImageFormat::FORMAT_RGBA16F, EImageFormat::FORMAT_RGBA16F, depthPrePassTextureFormat},
            .CullMode       = ECullMode::CULL_MODE_BACK,
            .bMeshShading   = true,
            .bBlendEnable   = true,
            .BlendMode      = EBlendMode::BLEND_MODE_ALPHA,
            .bDepthTest     = true,
            .bDepthWrite    = false,
            .DepthCompareOp = ECompareOp::COMPARE_OP_EQUAL};

        PipelineSpecification forwardPlusPS = {.DebugName       = "ForwardPlusOpaque",
                                               .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(forwardPlusGPO),
                                               .Shader          = ShaderLibrary::Get("ForwardPlus"),
                                               .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};

        s_RendererData->ForwardPlusOpaquePipelineHash = PipelineLibrary::Push(forwardPlusPS);

        forwardPlusPS.DebugName                                                                 = "ForwardPlusTransparent";
        std::get<GraphicsPipelineOptions>(forwardPlusPS.PipelineOptions.value()).DepthCompareOp = reversedDepthCompareOp;
        s_RendererData->ForwardPlusTransparentPipelineHash                                      = PipelineLibrary::Push(forwardPlusPS);
    }

    // SSAO
    {
        // Gbuffer goes as last, so there's no way for SSAO to get it.
        // ssaoPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        const GraphicsPipelineOptions ssaoGPO = {.Formats = {EImageFormat::FORMAT_R8_UNORM}, .CullMode = ECullMode::CULL_MODE_BACK};

        PipelineSpecification ssaoPS = {.DebugName       = "SSAO",
                                        .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(ssaoGPO),
                                        .Shader          = ShaderLibrary::Get("AO/SSAO"),
                                        .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};

        s_RendererData->SSAOPipelineHash = PipelineLibrary::Push(ssaoPS);
    }

    // BoxBlurAO
    {
        const GraphicsPipelineOptions boxBlurAOGPO = {.Formats = {EImageFormat::FORMAT_R8_UNORM}, .CullMode = ECullMode::CULL_MODE_BACK};

        PipelineSpecification boxBlurAOPS = {.DebugName       = "BoxBlurAO",
                                             .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(boxBlurAOGPO),
                                             .Shader          = ShaderLibrary::Get("Post/BoxBlur"),
                                             .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};

        s_RendererData->BoxBlurAOPipelineHash = PipelineLibrary::Push(boxBlurAOPS);
    }

    // Screen Space Shadows
    {
        PipelineSpecification sssPS = {.DebugName       = "ScrenSpaceShadows",
                                       .PipelineOptions = MakeOptional<ComputePipelineOptions>(),
                                       .Shader          = ShaderLibrary::Get("SSShadows"),
                                       .PipelineType    = EPipelineType::PIPELINE_TYPE_COMPUTE};

        s_RendererData->SSShadowsPipelineHash = PipelineLibrary::Push(sssPS);
    }

    // Bloom
    {
        for (uint32_t i{}; i < 2; ++i)
        {
            const GraphicsPipelineOptions bloomGPO = {.Formats = {EImageFormat::FORMAT_RGBA16F}, .CullMode = ECullMode::CULL_MODE_BACK};

            const std::string blurTypeStr = (i == 0) ? "Horiz" : "Vert";
            PipelineSpecification bloomPS = {.DebugName       = "Bloom" + blurTypeStr,
                                             .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(bloomGPO),
                                             .Shader          = ShaderLibrary::Get("Post/GaussianBlur"),
                                             .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
            bloomPS.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(i);

            s_RendererData->BloomPipelineHash[i] = PipelineLibrary::Push(bloomPS);
        }
    }

    // Final Pass
    {
        const GraphicsPipelineOptions compositeGPO = {.Formats  = {EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32},
                                                      .CullMode = ECullMode::CULL_MODE_BACK};

        PipelineSpecification compositePipelineSpec = {.DebugName       = "Composite",
                                                       .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(compositeGPO),
                                                       .Shader          = ShaderLibrary::Get("Composite"),
                                                       .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};

        s_RendererData->CompositePipelineHash = PipelineLibrary::Push(compositePipelineSpec);
    }

    PipelineLibrary::Compile();
}

const std::map<std::string, Shared<Image>> Renderer::GetRenderTargetList()
{
    return {};
    PFR_ASSERT(false, "NOT IMPLEMENTED!");
    PFR_ASSERT(s_RendererData, "RendererData is not valid!");
    std::map<std::string, Shared<Image>> renderTargetList;

    // renderTargetList["GBuffer"]   = s_RendererData->GBuffer->GetAttachments()[0].Attachment;
    // renderTargetList["AO"]        = s_RendererData->AOFramebuffer->GetAttachments()[0].Attachment;
    // renderTargetList["BlurAO"]    = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].Attachment;
    // renderTargetList["Heatmap"]   = s_RendererData->LightHeatMapImage;
    // renderTargetList["Frustums"]  = s_RendererData->FrustumDebugImage;
    // renderTargetList["RayTrace"]  = s_RendererData->RTImage;
    // renderTargetList["SSShadows"] = s_RendererData->SSShadowsImage;

    return renderTargetList;
}

}  // namespace Pathfinder