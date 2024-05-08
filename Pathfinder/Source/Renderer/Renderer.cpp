#include "PathfinderPCH.h"
#include "Renderer.h"
#include "Renderer2D.h"

#include "Core/Application.h"
#include "Core/Threading.h"
#include "Swapchain.h"
#include "Core/Window.h"
#include "GraphicsContext.h"

#include "Shader.h"
#include "CommandBuffer.h"
#include "Pipeline.h"
#include "Texture2D.h"
#include "Material.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Camera/Camera.h"
#include "Mesh/Mesh.h"
#include "Mesh/Submesh.h"

#include "HWRT.h"
#include "RenderGraph/RenderGraphBuilder.h"

#include "Debug/DebugRenderer.h"

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

    s_RendererSettings.bVSync   = Application::Get().GetWindow()->IsVSync();
    s_RendererData->LightStruct = MakeUnique<LightData>();

    ShaderLibrary::Load({{"ForwardPlus"},
                         {"DepthPrePass"},
                         {"SSShadows"},
                         {"ObjectCulling"},
                         {"LightCulling"},
                         {"ComputeFrustums"},
                         {"Composite"},
                         {"AO/SSAO"},
                         {"AO/HBAO"},
                         {"AtmosphericScattering"},
                         {"Post/GaussianBlur"},
                         {"Post/MedianBlur"},
                         {"Post/BoxBlur"},
                         {"RayTrace"}});

    std::ranges::for_each(s_RendererData->UploadHeap,
                          [](auto& uploadHeap)
                          {
                              BufferSpecification uploadHeapSpec = {EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE};
                              uploadHeapSpec.bBindlessUsage      = false;
                              uploadHeapSpec.bMapPersistent      = true;
                              uploadHeapSpec.BufferCapacity      = s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY;

                              uploadHeap = Buffer::Create(uploadHeapSpec);
                          });

    std::ranges::for_each(s_RendererData->CameraSSBO,
                          [](auto& cameraSSBO)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                            EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              ubSpec.BufferCapacity      = sizeof(s_RendererData->CameraStruct);

                              cameraSSBO = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->LightsSSBO,
                          [](auto& lightsSSBO)
                          {
                              BufferSpecification lightsSSBOSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                                    EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              lightsSSBOSpec.BufferCapacity      = sizeof(LightData);
                              lightsSSBO                         = Buffer::Create(lightsSSBOSpec);
                          });
    {
        CommandBufferSpecification cbSpec = {};
        cbSpec.Level                      = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY;
        cbSpec.ThreadID                   = JobSystem::MapThreadID(JobSystem::GetMainThreadID());

        for (uint32_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
        {
            cbSpec.FrameIndex = frameIndex;

            cbSpec.Type                                     = ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS;
            s_RendererData->RenderCommandBuffer[frameIndex] = CommandBuffer::Create(cbSpec);

            cbSpec.Type                                      = ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE;
            s_RendererData->ComputeCommandBuffer[frameIndex] = CommandBuffer::Create(cbSpec);

            cbSpec.Type                                       = ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER;
            s_RendererData->TransferCommandBuffer[frameIndex] = CommandBuffer::Create(cbSpec);
        }
    }

    {
        TextureSpecification whiteTextureSpec = {1, 1};
        whiteTextureSpec.bBindlessUsage       = true;
        uint32_t whiteColor                   = 0xFFFFFFFF;
        s_RendererData->WhiteTexture          = Texture2D::Create(whiteTextureSpec, &whiteColor, sizeof(whiteColor));
    }

    {
        // Initialize buffers(will be resized by engine needs.)

        {
            BufferSpecification bufferSpec   = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                                EBufferUsage::BUFFER_USAGE_INDIRECT};
            bufferSpec.BufferCapacity        = sizeof(uint32_t) + 1 * sizeof(DrawMeshTasksIndirectCommand);
            bufferSpec.bMapPersistent        = true;
            s_RendererData->DrawBufferOpaque = Buffer::Create(bufferSpec);
        }

        {
            BufferSpecification bufferSpec   = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_DATA_OPAQUE_BINDING, true};
            bufferSpec.BufferCapacity        = 1 * sizeof(MeshData);
            s_RendererData->MeshesDataOpaque = Buffer::Create(bufferSpec);
        }

        {
            BufferSpecification bufferSpec           = {EBufferUsage::BUFFER_USAGE_STORAGE};
            bufferSpec.BufferCapacity                = 1 * sizeof(uint32_t);
            s_RendererData->CulledMeshesBufferOpaque = Buffer::Create(bufferSpec);
        }

        {
            BufferSpecification bufferSpec        = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                                     EBufferUsage::BUFFER_USAGE_INDIRECT};
            bufferSpec.BufferCapacity             = sizeof(uint32_t) + 1 * sizeof(DrawMeshTasksIndirectCommand);
            bufferSpec.bMapPersistent             = true;
            s_RendererData->DrawBufferTransparent = Buffer::Create(bufferSpec);
        }

        {
            BufferSpecification bufferSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_DATA_TRANSPARENT_BINDING, true};
            bufferSpec.BufferCapacity      = 1 * sizeof(MeshData);
            s_RendererData->MeshesDataTransparent = Buffer::Create(bufferSpec);
        }

        {
            BufferSpecification bufferSpec                = {EBufferUsage::BUFFER_USAGE_STORAGE};
            bufferSpec.BufferCapacity                     = 1 * sizeof(uint32_t);
            s_RendererData->CulledMeshesBufferTransparent = Buffer::Create(bufferSpec);
        }

        PipelineSpecification objectCullingPipelineSpec = {"ObjectCulling"};
        objectCullingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        objectCullingPipelineSpec.PipelineOptions       = std::make_optional<ComputePipelineOptions>();
        objectCullingPipelineSpec.Shader                = ShaderLibrary::Get("ObjectCulling");
        objectCullingPipelineSpec.bBindlessCompatible   = true;
        PipelineBuilder::Push(s_RendererData->ObjectCullingPipeline, objectCullingPipelineSpec);
    }

    {
        FramebufferSpecification framebufferSpec = {"DepthPrePass"};

        FramebufferAttachmentSpecification depthAttachmentSpec = {EImageFormat::FORMAT_D32F,
                                                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                  ELoadOp::CLEAR,
                                                                  EStoreOp::STORE,
                                                                  glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
                                                                  false,
                                                                  ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                  ESamplerFilter::SAMPLER_FILTER_NEAREST,
                                                                  true};

        framebufferSpec.Attachments.emplace_back(depthAttachmentSpec);
        s_RendererData->DepthPrePassFramebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                          { s_RendererData->DepthPrePassFramebuffer->Resize(width, height); });
    }

    {
#if 0
        {
            PipelineSpecification rtPipelineSpec = {"RayTracing"};
            rtPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_RAY_TRACING;
            rtPipelineSpec.bBindlessCompatible   = true;
            rtPipelineSpec.Shader                = ShaderLibrary::Get("RayTrace");

            RayTracingPipelineOptions rtPipelineOptions = {};
            rtPipelineSpec.PipelineOptions              = std::make_optional<RayTracingPipelineOptions>(rtPipelineOptions);

            s_RendererData->RTPipeline = Pipeline::Create(rtPipelineSpec);
        }
        s_RendererData->RTSBT = ShaderLibrary::Get("RayTrace")->CreateSBT(s_RendererData->RTPipeline);

        {
            PipelineSpecification rtComputePipelineSpec = {"RayTracingCompute"};
            rtComputePipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
            rtComputePipelineSpec.bBindlessCompatible   = true;
            rtComputePipelineSpec.Shader                = ShaderLibrary::Get("RayTrace");

            ComputePipelineOptions rtComputePipelineOptions = {};
            rtComputePipelineSpec.PipelineOptions           = std::make_optional<ComputePipelineOptions>(rtComputePipelineOptions);

            PipelineBuilder::Push(s_RendererData->RTComputePipeline, rtComputePipelineSpec);
        }
#endif

        ImageSpecification rtImageSpec = {};
        rtImageSpec.bBindlessUsage     = true;
        rtImageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        rtImageSpec.UsageFlags =
            EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT;
        s_RendererData->RTImage = Image::Create(rtImageSpec);

        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                          { s_RendererData->RTImage->Resize(width, height); });
    }

    // NOTE: Reversed-Z, summary:https://ajweeks.com/blog/2019/04/06/ReverseZ/
    PipelineSpecification depthPrePassPipelineSpec = {"DepthPrePass"};
    depthPrePassPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    depthPrePassPipelineSpec.Shader                = ShaderLibrary::Get("DepthPrePass");
    depthPrePassPipelineSpec.bBindlessCompatible   = true;

    GraphicsPipelineOptions depthGPO         = {};
    depthGPO.CullMode                        = ECullMode::CULL_MODE_BACK;
    depthGPO.TargetFramebuffer               = s_RendererData->DepthPrePassFramebuffer;
    depthGPO.bMeshShading                    = true;
    depthGPO.bDepthTest                      = true;
    depthGPO.bDepthWrite                     = true;
    depthGPO.DepthCompareOp                  = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    depthPrePassPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(depthGPO);
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
                                                                 ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                                                 true};
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
                                                                 ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                                                 true};
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
        PipelineSpecification forwardPipelineSpec = {"ForwardPlusOpaque"};
        forwardPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        forwardPipelineSpec.Shader                = ShaderLibrary::Get("ForwardPlus");
        // Rendering view normals useless for now.
        // forwardPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        forwardPipelineSpec.bBindlessCompatible = true;

        GraphicsPipelineOptions forwardGPO  = {};
        forwardGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        forwardGPO.bDepthTest               = true;
        forwardGPO.bDepthWrite              = false;
        forwardGPO.DepthCompareOp           = ECompareOp::COMPARE_OP_EQUAL;
        forwardGPO.TargetFramebuffer        = s_RendererData->GBuffer;
        forwardGPO.bMeshShading             = true;
        forwardGPO.bBlendEnable             = true;
        forwardGPO.BlendMode                = EBlendMode::BLEND_MODE_ALPHA;
        forwardPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(forwardGPO);

        PipelineBuilder::Push(s_RendererData->ForwardPlusOpaquePipeline, forwardPipelineSpec);

        forwardPipelineSpec.DebugName = "ForwardPlusTransparent";
        std::get<GraphicsPipelineOptions>(forwardPipelineSpec.PipelineOptions.value()).DepthCompareOp =
            std::get<GraphicsPipelineOptions>(depthPrePassPipelineSpec.PipelineOptions.value()).DepthCompareOp;
        PipelineBuilder::Push(s_RendererData->ForwardPlusTransparentPipeline, forwardPipelineSpec);
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
        Application::Get().GetWindow()->AddResizeCallback([&](uint32_t width, uint32_t height)
                                                          { s_RendererData->CompositeFramebuffer->Resize(width, height); });

        PipelineSpecification compositePipelineSpec = {"Composite"};
        compositePipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        compositePipelineSpec.Shader                = ShaderLibrary::Get("Composite");
        compositePipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions compositeGPO  = {};
        compositeGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        compositeGPO.TargetFramebuffer        = s_RendererData->CompositeFramebuffer;
        compositePipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(compositeGPO);

        PipelineBuilder::Push(s_RendererData->CompositePipeline, compositePipelineSpec);
    }

    Renderer2D::Init();
#if PFR_DEBUG
    DebugRenderer::Init();
#endif

    // Light-Culling
    {
        PipelineSpecification lightCullingPipelineSpec = {"LightCulling"};
        lightCullingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        lightCullingPipelineSpec.Shader                = ShaderLibrary::Get("LightCulling");
        lightCullingPipelineSpec.bBindlessCompatible   = true;
        lightCullingPipelineSpec.PipelineOptions       = std::make_optional<ComputePipelineOptions>();
        PipelineBuilder::Push(s_RendererData->LightCullingPipeline, lightCullingPipelineSpec);

        PipelineSpecification computeFrustumsPipelineSpec = {"ComputeFrustums"};
        computeFrustumsPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        computeFrustumsPipelineSpec.Shader                = ShaderLibrary::Get("ComputeFrustums");
        computeFrustumsPipelineSpec.bBindlessCompatible   = true;
        computeFrustumsPipelineSpec.PipelineOptions       = std::make_optional<ComputePipelineOptions>();
        PipelineBuilder::Push(s_RendererData->ComputeFrustumsPipeline, computeFrustumsPipelineSpec);
    }

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize   = MAX_POINT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * width * height;
        sbSpec.BufferCapacity = sbSize;

        s_RendererData->PointLightIndicesSSBO = Buffer::Create(sbSpec);
    }

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize   = MAX_SPOT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * width * height;
        sbSpec.BufferCapacity = sbSize;

        s_RendererData->SpotLightIndicesSSBO = Buffer::Create(sbSpec);
    }

    {
        BufferSpecification frustumsSBSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize           = sizeof(TileFrustum) * width * height;
        frustumsSBSpec.BufferCapacity = sbSize;

        s_RendererData->FrustumsSSBO = Buffer::Create(frustumsSBSpec);
    }

    {
        ImageSpecification imageSpec = {};
        imageSpec.bBindlessUsage     = true;
        imageSpec.Format             = EImageFormat::FORMAT_RGBA8_UNORM;
        imageSpec.UsageFlags =
            EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT;

        s_RendererData->FrustumDebugImage = Image::Create(imageSpec);
        s_RendererData->LightHeatMapImage = Image::Create(imageSpec);
    }

    Application::Get().GetWindow()->AddResizeCallback(
        [](uint32_t width, uint32_t height)
        {
            s_RendererData->bNeedsFrustumsRecomputing = true;
            s_RendererData->FrustumDebugImage->Resize(width, height);

            s_RendererData->LightHeatMapImage->Resize(width, height);

            const uint32_t adjustedTiledWidth =
                DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
            const uint32_t adjustedTiledHeight =
                DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);

            {
                const size_t sbSize = MAX_POINT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;

                s_RendererData->PointLightIndicesSSBO->Resize(sbSize);
            }

            {
                const size_t sbSize = MAX_SPOT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * adjustedTiledWidth * adjustedTiledHeight;

                s_RendererData->SpotLightIndicesSSBO->Resize(sbSize);
            }

            {
                const size_t sbSize = sizeof(TileFrustum) * adjustedTiledWidth * adjustedTiledHeight;

                s_RendererData->FrustumsSSBO->Resize(sbSize);
            }
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
                                                                     ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                                                     true};

        framebufferSpec.Attachments.emplace_back(aoAttachmentSpec);
        s_RendererData->SSAO.Framebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->SSAO.Framebuffer->Resize(width, height); });

        PipelineSpecification ssaoPipelineSpec = {"SSAO"};
        ssaoPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        ssaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO");
        ssaoPipelineSpec.bBindlessCompatible   = true;

        // Gbuffer goes as last, so there's no way for SSAO to get it.
        // ssaoPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        GraphicsPipelineOptions ssaoGPO  = {};
        ssaoGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        ssaoGPO.TargetFramebuffer        = s_RendererData->SSAO.Framebuffer;
        ssaoPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(ssaoGPO);

        PipelineBuilder::Push(s_RendererData->SSAO.Pipeline, ssaoPipelineSpec);
    }

    // Noise texture for ambient occlusions
    {
        const auto& appSpec       = Application::Get().GetSpecification();
        const auto rotTexturePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / "Other/rot_texture.bmp";
        int32_t x = 0, y = 0, channels = 0;
        void* rawImageData  = ImageUtils::LoadRawImage(rotTexturePath, false, &x, &y, &channels);
        void* rgbaImageData = ImageUtils::ConvertRgbToRgba((uint8_t*)rawImageData, x, y);

        TextureSpecification aoNoiseTextureSpec = {x, y};
        aoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA32F;
        aoNoiseTextureSpec.bBindlessUsage       = true;
        aoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_NEAREST;
        aoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
        s_RendererData->AONoiseTexture          = Texture2D::Create(aoNoiseTextureSpec, rgbaImageData, x * y * sizeof(glm::vec4));

        free(rgbaImageData);
        ImageUtils::UnloadRawImage(rawImageData);
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

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->HBAO.Framebuffer->Resize(width, height); });

        PipelineSpecification hbaoPipelineSpec = {"HBAO"};
        hbaoPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        hbaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/HBAO");
        hbaoPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions hbaoGPO  = {};
        hbaoGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        hbaoGPO.TargetFramebuffer        = s_RendererData->HBAO.Framebuffer;
        hbaoPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(hbaoGPO);

        PipelineBuilder::Push(s_RendererData->HBAO.Pipeline, hbaoPipelineSpec);
    }

    {
        // Ping-pong framebuffers
        for (uint32_t i{}; i < s_RendererData->BlurAOFramebuffer.size(); ++i)
        {
            const std::string blurTypeStr            = (i == 0) ? "Horiz" : "Vert";
            FramebufferSpecification framebufferSpec = {"BlurAOFramebuffer" + blurTypeStr};
            auto& bloomAttachmentSpec                = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_R8_UNORM, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ELoadOp::CLEAR, EStoreOp::STORE,
                glm::vec4(1.0f, glm::vec3(0.0f)), false, ESamplerWrap::SAMPLER_WRAP_REPEAT, ESamplerFilter::SAMPLER_FILTER_LINEAR, true);
            s_RendererData->BlurAOFramebuffer[i] = Framebuffer::Create(framebufferSpec);

            PipelineSpecification blurAOPipelineSpec = {"BlurAO" + blurTypeStr};
            blurAOPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            blurAOPipelineSpec.Shader                = ShaderLibrary::Get("Post/GaussianBlur");
            blurAOPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(i);
            blurAOPipelineSpec.bBindlessCompatible = true;

            GraphicsPipelineOptions blurAOGPO  = {};
            blurAOGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
            blurAOGPO.TargetFramebuffer        = s_RendererData->BlurAOFramebuffer[i];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGPO);

            PipelineBuilder::Push(s_RendererData->BlurAOPipeline[i], blurAOPipelineSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height) {
                std::ranges::for_each(s_RendererData->BlurAOFramebuffer,
                                      [&](const auto& framebuffer) { framebuffer->Resize(width, height); });
            });

        {
            PipelineSpecification blurAOPipelineSpec = {"MedianBlurAO"};
            blurAOPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            blurAOPipelineSpec.Shader                = ShaderLibrary::Get("Post/MedianBlur");
            blurAOPipelineSpec.bBindlessCompatible   = true;

            GraphicsPipelineOptions blurAOGPO  = {};
            blurAOGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
            blurAOGPO.TargetFramebuffer        = s_RendererData->BlurAOFramebuffer[1];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGPO);

            PipelineBuilder::Push(s_RendererData->MedianBlurAOPipeline, blurAOPipelineSpec);
        }

        {
            PipelineSpecification blurAOPipelineSpec = {"BoxBlurAO"};
            blurAOPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            blurAOPipelineSpec.Shader                = ShaderLibrary::Get("Post/BoxBlur");
            blurAOPipelineSpec.bBindlessCompatible   = true;

            GraphicsPipelineOptions blurAOGPO  = {};
            blurAOGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
            blurAOGPO.TargetFramebuffer        = s_RendererData->BlurAOFramebuffer[1];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGPO);

            PipelineBuilder::Push(s_RendererData->BoxBlurAOPipeline, blurAOPipelineSpec);
        }
    }

    {
        ImageSpecification sssImageSpec = {};
        sssImageSpec.Format             = EImageFormat::FORMAT_R32F;
        sssImageSpec.Layout             = EImageLayout::IMAGE_LAYOUT_GENERAL;
        sssImageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_STORAGE_BIT | EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                  EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;
        sssImageSpec.bBindlessUsage    = true;
        s_RendererData->SSShadowsImage = Image::Create(sssImageSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->SSShadowsImage->Resize(width, height); });

        PipelineSpecification sssPipelineSpec = {"ScrenSpaceShadows"};
        sssPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        sssPipelineSpec.Shader                = ShaderLibrary::Get("SSShadows");
        sssPipelineSpec.bBindlessCompatible   = true;
        sssPipelineSpec.PipelineOptions       = std::make_optional<ComputePipelineOptions>();
        PipelineBuilder::Push(s_RendererData->SSShadowsPipeline, sssPipelineSpec);
    }

    {
        PipelineSpecification asPipelineSpec = {"AtmosphericScattering"};
        asPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        asPipelineSpec.Shader                = ShaderLibrary::Get("AtmosphericScattering");
        asPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions asGPO  = {};
        asGPO.TargetFramebuffer        = s_RendererData->GBuffer;
        asGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        asPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(asGPO);
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
                glm::vec4(0.0f), false, ESamplerWrap::SAMPLER_WRAP_REPEAT, ESamplerFilter::SAMPLER_FILTER_LINEAR, true);
            s_RendererData->BloomFramebuffer[i] = Framebuffer::Create(framebufferSpec);

            PipelineSpecification bloomPipelineSpec = {"Bloom" + blurTypeStr};
            bloomPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            bloomPipelineSpec.Shader                = ShaderLibrary::Get("Post/GaussianBlur");
            bloomPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(i);
            bloomPipelineSpec.bBindlessCompatible = true;

            GraphicsPipelineOptions bloomGraphicsPipelineOptions = {};
            bloomGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->BloomFramebuffer[i];
            bloomGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
            bloomPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(bloomGraphicsPipelineOptions);

            PipelineBuilder::Push(s_RendererData->BloomPipeline[i], bloomPipelineSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height) {
                std::ranges::for_each(s_RendererData->BloomFramebuffer,
                                      [&](const auto& framebuffer) { framebuffer->Resize(width, height); });
            });
    }

    /* CREATE STAGE */
    PipelineBuilder::Build();
    ShaderLibrary::DestroyGarbageIfNeeded();
    /* SHADER UPDATE STAGE */

    s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                          s_RendererData->CulledMeshesBufferOpaque);
    s_RendererData->ForwardPlusOpaquePipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                              s_RendererData->CulledMeshesBufferOpaque);
    s_RendererData->DepthPrePassPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                         s_RendererData->CulledMeshesBufferOpaque);
    s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_DrawBufferOpaque", s_RendererData->DrawBufferOpaque);

    s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferTransparent",
                                                                          s_RendererData->CulledMeshesBufferTransparent);
    s_RendererData->ForwardPlusTransparentPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferTransparent",
                                                                                   s_RendererData->CulledMeshesBufferTransparent);
    s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_DrawBufferTransparent", s_RendererData->DrawBufferTransparent);

    s_RendererData->RenderGraph = RenderGraphBuilder::Create("Other/RenderGraphDescription.pfr");

    LOG_TAG_TRACE(RENDERER_3D, "Renderer3D created!");
}

void Renderer::Shutdown()
{
#if PFR_DEBUG
    DebugRenderer::Shutdown();
#endif

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
    // Update VSync state.
    auto& window = Application::Get().GetWindow();
    window->SetVSync(s_RendererSettings.bVSync);

    Timer t                              = {};
    s_RendererData->bAnybodyCastsShadows = false;
    s_RendererData->LastBoundPipeline.reset();
    s_RendererData->PassStats.clear();
    s_RendererData->PipelineStats.clear();

    s_RendererData->FrameIndex                   = window->GetCurrentFrameIndex();
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

    s_RendererData->LightStruct->DirectionalLightCount = 0;
    s_RendererData->LightStruct->SpotLightCount        = 0;
    s_RendererData->LightStruct->PointLightCount       = 0;

    {
        const auto& renderTimestamps = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetTimestampsResults();
        s_RendererStats.GPUTime      = std::accumulate(renderTimestamps.begin(), renderTimestamps.end(), s_RendererStats.GPUTime);

        const auto& renderPipelineStats  = s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();
        const auto& computePipelineStats = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->GetPipelineStatisticsResults();

        s_RendererData->PassStats["ObjectCullingPass"]         = renderTimestamps[0];
        s_RendererData->PassStats["RayTracingPass"]            = renderTimestamps[1];
        s_RendererData->PassStats["DepthPrePass"]              = renderTimestamps[2];
        s_RendererData->PassStats["ScreenSpaceShadowsPass"]    = renderTimestamps[3];
        s_RendererData->PassStats["AtmosphericScatteringPass"] = renderTimestamps[4];
        s_RendererData->PassStats["SSAOPass"]                  = renderTimestamps[5];
        s_RendererData->PassStats["HBAOPass"]                  = renderTimestamps[6];
        s_RendererData->PassStats["BlurAOPass"]                = renderTimestamps[7];
        s_RendererData->PassStats["ComputeFrustumsPass"]       = renderTimestamps[8];
        s_RendererData->PassStats["LightCullingPass"]          = renderTimestamps[9];
        s_RendererData->PassStats["GeometryPass"]              = renderTimestamps[10];
        s_RendererData->PassStats["BloomPass"]                 = renderTimestamps[11];
        s_RendererData->PassStats["CompositePass"]             = renderTimestamps[12];

        std::ranges::for_each(renderPipelineStats,
                              [](const auto& pipelineStat)
                              {
                                  const auto pipelineStatStr =
                                      std::string(CommandBuffer::ConvertQueryPipelineStatisticToCString(pipelineStat.first));
                                  s_RendererData->PipelineStats[pipelineStatStr] = pipelineStat.second;
                              });
    }

    if (s_RendererData->DrawBufferOpaque->GetMapped())
        s_RendererData->ObjectCullStats.DrawCountOpaque = *(uint32_t*)s_RendererData->DrawBufferOpaque->GetMapped();
    if (s_RendererData->DrawBufferTransparent->GetMapped())
        s_RendererData->ObjectCullStats.DrawCountTransparent = *(uint32_t*)s_RendererData->DrawBufferTransparent->GetMapped();

    s_RendererStats.ObjectsDrawn = s_RendererData->ObjectCullStats.DrawCountOpaque + s_RendererData->ObjectCullStats.DrawCountTransparent;

    // FIXME queue family indices thing
    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    Renderer2D::Begin();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);

    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);

    GraphicsContext::Get().FillMemoryBudgetStats(s_RendererStats.MemoryBudgets);
    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
}

void Renderer::Flush(const Unique<UILayer>& uiLayer)
{
    Timer t = {};
    s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->SetData(s_RendererData->LightStruct.get(), sizeof(LightData));
    s_BindlessRenderer->UpdateDataIfNeeded();

    ObjectCullingPass();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ);
    RayTracingPass();

    s_RendererData->GBuffer->Clear(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    DepthPrePass();
    ScreenSpaceShadowsPass();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
        EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ);

    AtmosphericScatteringPass();

    SSAOPass();
    HBAOPass();
    BlurAOPass();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ);
    ComputeFrustumsPass();
    LightCullingPass();

    GeometryPass();

#if PFR_DEBUG
    DebugRenderer::Flush();
#endif

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

    if (uiLayer) uiLayer->EndRender();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    const auto& swapchain = Application::Get().GetWindow()->GetSwapchain();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit(
        false, false, UINT64_MAX,
        {EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT},
        {swapchain->GetImageAvailableSemaphore(), cts, tts}, {1, ctsVal, ttsVal}, {swapchain->GetRenderSemaphore()}, {},
        swapchain->GetRenderFence());

    s_RendererData->CurrentRenderCommandBuffer.reset();
    s_RendererData->CurrentComputeCommandBuffer.reset();
    s_RendererData->CurrentTransferCommandBuffer.reset();

    s_RendererData->OpaqueObjects.clear();
    s_RendererData->TransparentObjects.clear();

    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
}

void Renderer::BeginScene(const Camera& camera)
{
    if (s_RendererData->CameraStruct.FOV != camera.GetZoom()) s_RendererData->bNeedsFrustumsRecomputing = true;
    s_RendererData->CameraStruct = {camera.GetFrustum(),
                                    camera.GetProjection(),
                                    camera.GetView(),
                                    camera.GetViewProjection(),
                                    camera.GetInverseProjection(),
                                    camera.GetPosition(),
                                    camera.GetNearPlaneDepth(),
                                    camera.GetFarPlaneDepth(),
                                    camera.GetZoom(),
                                    glm::vec2(s_RendererData->CompositeFramebuffer->GetSpecification().Width,
                                              s_RendererData->CompositeFramebuffer->GetSpecification().Height),
                                    glm::vec2(1.f / s_RendererData->CompositeFramebuffer->GetSpecification().Width,
                                              1.f / s_RendererData->CompositeFramebuffer->GetSpecification().Height)};

    s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->SetData(&s_RendererData->CameraStruct, sizeof(s_RendererData->CameraStruct));
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

void Renderer::ObjectCullingPass()
{
    if (IsWorldEmpty()) return;

    // 1. Sort them front to back, to minimize overdraw.
    std::sort(std::execution::par, s_RendererData->OpaqueObjects.begin(), s_RendererData->OpaqueObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::length(lhsPos - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhsPos - s_RendererData->CameraStruct.Position);

                  return lhsDist < rhsDist;  // Front to back drawing(minimize overdraw)
              });

    // 1. Sort them back to front.
    std::sort(std::execution::par, s_RendererData->TransparentObjects.begin(), s_RendererData->TransparentObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const glm::vec3 lhsPos = glm::vec3(lhs.Transform[3]);
                  const glm::vec3 rhsPos = glm::vec3(rhs.Transform[3]);

                  const float lhsDist = glm::length(lhsPos - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhsPos - s_RendererData->CameraStruct.Position);

                  return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
              });

    // Prepare indirect draw buffer for both opaque and transparent objects.

    if ((s_RendererData->DrawBufferOpaque->GetSpecification().BufferCapacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand) <
        s_RendererData->OpaqueObjects.size())
    {
        s_RendererData->DrawBufferOpaque->Resize(sizeof(uint32_t) +
                                                 s_RendererData->OpaqueObjects.size() * sizeof(DrawMeshTasksIndirectCommand));
        s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_DrawBufferOpaque", s_RendererData->DrawBufferOpaque);
    }

    if (s_RendererData->MeshesDataOpaque->GetSpecification().BufferCapacity / sizeof(MeshData) < s_RendererData->OpaqueObjects.size())
        s_RendererData->MeshesDataOpaque->Resize(s_RendererData->OpaqueObjects.size() * sizeof(MeshData));

    std::vector<MeshData> opaqueMeshesData = {};
    for (const auto& [submesh, transform] : s_RendererData->OpaqueObjects)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(transform, scale, rotation, translation, skew, perspective);
        auto& meshData = opaqueMeshesData.emplace_back(
            translation, scale, glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w), submesh->GetBoundingSphere(),
            submesh->GetVertexPositionBuffer()->GetBindlessIndex(), submesh->GetVertexAttributeBuffer()->GetBindlessIndex(),
            submesh->GetIndexBuffer()->GetBindlessIndex(), submesh->GetMeshletVerticesBuffer()->GetBindlessIndex(),
            submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex(), submesh->GetMeshletBuffer()->GetBindlessIndex(),
            submesh->GetMaterial()->GetBufferIndex());
    }
    s_RendererData->MeshesDataOpaque->SetData(opaqueMeshesData.data(), opaqueMeshesData.size() * sizeof(opaqueMeshesData[0]));

    if (s_RendererData->CulledMeshesBufferOpaque->GetSpecification().BufferCapacity / sizeof(uint32_t) <
        s_RendererData->OpaqueObjects.size())
    {
        s_RendererData->CulledMeshesBufferOpaque->Resize(s_RendererData->OpaqueObjects.size() * sizeof(uint32_t));

        s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                              s_RendererData->CulledMeshesBufferOpaque);

        s_RendererData->DepthPrePassPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                             s_RendererData->CulledMeshesBufferOpaque);
        s_RendererData->ForwardPlusOpaquePipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferOpaque",
                                                                                  s_RendererData->CulledMeshesBufferOpaque);
    }

    if ((s_RendererData->DrawBufferTransparent->GetSpecification().BufferCapacity - sizeof(uint32_t)) /
            sizeof(DrawMeshTasksIndirectCommand) <
        s_RendererData->TransparentObjects.size())
    {
        s_RendererData->DrawBufferTransparent->Resize(sizeof(uint32_t) +
                                                      s_RendererData->TransparentObjects.size() * sizeof(DrawMeshTasksIndirectCommand));

        s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_DrawBufferTransparent",
                                                                              s_RendererData->DrawBufferTransparent);
    }

    if (s_RendererData->MeshesDataTransparent->GetSpecification().BufferCapacity / sizeof(MeshData) <
        s_RendererData->TransparentObjects.size())
        s_RendererData->MeshesDataTransparent->Resize(s_RendererData->TransparentObjects.size() * sizeof(MeshData));

    std::vector<MeshData> transparentMeshesData = {};
    for (const auto& [submesh, transform] : s_RendererData->TransparentObjects)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(transform, scale, rotation, translation, skew, perspective);
        auto& meshData = transparentMeshesData.emplace_back(
            translation, scale, glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w), submesh->GetBoundingSphere(),
            submesh->GetVertexPositionBuffer()->GetBindlessIndex(), submesh->GetVertexAttributeBuffer()->GetBindlessIndex(),
            submesh->GetIndexBuffer()->GetBindlessIndex(), submesh->GetMeshletVerticesBuffer()->GetBindlessIndex(),
            submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex(), submesh->GetMeshletBuffer()->GetBindlessIndex(),
            submesh->GetMaterial()->GetBufferIndex());
    }
    if (!transparentMeshesData.empty())
        s_RendererData->MeshesDataTransparent->SetData(transparentMeshesData.data(),
                                                       transparentMeshesData.size() * sizeof(transparentMeshesData[0]));

    if (s_RendererData->CulledMeshesBufferTransparent->GetSpecification().BufferCapacity / sizeof(uint32_t) <
        s_RendererData->TransparentObjects.size())
    {
        s_RendererData->CulledMeshesBufferTransparent->Resize(s_RendererData->TransparentObjects.size() * sizeof(uint32_t));

        s_RendererData->ObjectCullingPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferTransparent",
                                                                              s_RendererData->CulledMeshesBufferTransparent);

        s_RendererData->ForwardPlusTransparentPipeline->GetSpecification().Shader->Set("s_CulledMeshIDBufferTransparent",
                                                                                       s_RendererData->CulledMeshesBufferTransparent);
    }

    // Clear buffers for indirect fulfilling.
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->DrawBufferOpaque, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferOpaque, EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EAccessFlags::ACCESS_TRANSFER_WRITE, EAccessFlags::ACCESS_SHADER_WRITE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->CulledMeshesBufferOpaque, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferOpaque, EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE, EAccessFlags::ACCESS_SHADER_WRITE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->DrawBufferTransparent, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferTransparent, EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE, EAccessFlags::ACCESS_SHADER_WRITE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->CulledMeshesBufferTransparent, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferTransparent, EPipelineStage::PIPELINE_STAGE_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE, EAccessFlags::ACCESS_SHADER_WRITE);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("ObjectCullingPass", glm::vec3(0.1f, 0.1f, 0.1f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    // 1. Opaque
    PushConstantBlock pc   = {};
    pc.CameraDataBuffer    = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.StorageImageIndex   = s_RendererData->OpaqueObjects.size();
    pc.MaterialBufferIndex = 0;
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ObjectCullingPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ObjectCullingPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        DivideToNextMultiple(s_RendererData->OpaqueObjects.size(), MESHLET_LOCAL_GROUP_SIZE), 1);

    // 2. Transparents
    pc.StorageImageIndex   = s_RendererData->TransparentObjects.size();
    pc.MaterialBufferIndex = 1;
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ObjectCullingPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ObjectCullingPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        DivideToNextMultiple(s_RendererData->TransparentObjects.size(), MESHLET_LOCAL_GROUP_SIZE), 1);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferOpaque, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT, EAccessFlags::ACCESS_SHADER_WRITE, EAccessFlags::ACCESS_INDIRECT_COMMAND_READ);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferTransparent, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT, EAccessFlags::ACCESS_SHADER_WRITE, EAccessFlags::ACCESS_INDIRECT_COMMAND_READ);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::DepthPrePass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->DepthPrePassFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));

    PushConstantBlock pc = {};
    pc.CameraDataBuffer  = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->DepthPrePassPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DepthPrePassPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferOpaque, sizeof(uint32_t), s_RendererData->DrawBufferOpaque, 0,
        (s_RendererData->DrawBufferOpaque->GetSpecification().BufferCapacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand),
        sizeof(DrawMeshTasksIndirectCommand));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

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
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
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

    PushConstantBlock pc  = {};
    pc.StorageImageIndex  = s_RendererData->AONoiseTexture->GetBindlessIndex();
    pc.AlbedoTextureIndex = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->HBAO.Pipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->HBAO.Pipeline, 0, 0, sizeof(pc),
                                                                                       &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->HBAO.Framebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BlurAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    PushConstantBlock pc  = {};
    pc.AlbedoTextureIndex = s_RendererData->SSAO.Framebuffer->GetAttachments()[0].m_Index;

    switch (s_RendererSettings.BlurType)
    {
        case EBlurType::BLUR_TYPE_GAUSSIAN:
        {
            for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
            {
                s_RendererData->BlurAOFramebuffer[i]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

                pc.AlbedoTextureIndex = (i == 0 ? s_RendererData->SSAO.Framebuffer->GetAttachments()[0].m_Index
                                                : s_RendererData->BlurAOFramebuffer.at(i - 1)->GetAttachments()[0].m_Index);

                BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->BlurAOPipeline[i]);
                s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->BlurAOPipeline[i], 0, 0,
                                                                                                   sizeof(pc), &pc);

                s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);
                s_RendererData->BlurAOFramebuffer[i]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
            }

            break;
        }

        case EBlurType::BLUR_TYPE_MEDIAN:
        {
            s_RendererData->BlurAOFramebuffer[1]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->MedianBlurAOPipeline);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->MedianBlurAOPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);
            s_RendererData->BlurAOFramebuffer[1]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            break;
        }

        case EBlurType::BLUR_TYPE_BOX:
        {
            s_RendererData->BlurAOFramebuffer[1]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->BoxBlurAOPipeline);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->BoxBlurAOPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);
            s_RendererData->BlurAOFramebuffer[1]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            break;
        }
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::ComputeFrustumsPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("ComputeFrustumsPass", glm::vec3(0.4f, 0.4f, 0.1f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    if (s_RendererData->bNeedsFrustumsRecomputing)
    {
        PushConstantBlock pc             = {};
        pc.StorageImageIndex             = s_RendererData->FrustumsSSBO->GetBindlessIndex();
        pc.CameraDataBuffer              = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightCullingFrustumDataBuffer = s_RendererData->FrustumsSSBO->GetBDA();

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ComputeFrustumsPipeline);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ComputeFrustumsPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        // Divide twice to make use of threads inside warps instead of creating frustum per warp.
        const auto& framebufferSpec = s_RendererData->DepthPrePassFramebuffer->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(framebufferSpec.Width, LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE),
            DivideToNextMultiple(framebufferSpec.Height, LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE));

        // Insert buffer memory barrier to prevent light culling pass reading until frustum computing is done.
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
            s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE, EAccessFlags::ACCESS_SHADER_READ);

        s_RendererData->bNeedsFrustumsRecomputing = false;
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::LightCullingPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("LightCullingPass", glm::vec3(0.9f, 0.9f, 0.2f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    if (s_RendererData->LightStruct->PointLightCount != 0 || s_RendererData->LightStruct->SpotLightCount != 0)
    {
        PushConstantBlock pc                  = {};
        pc.AlbedoTextureIndex                 = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;
        pc.StorageImageIndex                  = s_RendererData->LightHeatMapImage->GetBindlessIndex();
        pc.MeshIndexBufferIndex               = s_RendererData->FrustumDebugImage->GetBindlessIndex();
        pc.CameraDataBuffer                   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightDataBuffer                    = s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightCullingFrustumDataBuffer      = s_RendererData->FrustumsSSBO->GetBDA();
        pc.VisiblePointLightIndicesDataBuffer = s_RendererData->PointLightIndicesSSBO->GetBDA();
        pc.VisibleSpotLightIndicesDataBuffer  = s_RendererData->SpotLightIndicesSSBO->GetBDA();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE,
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ);

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->LightCullingPipeline);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->LightCullingPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        const auto& framebufferSpec = s_RendererData->DepthPrePassFramebuffer->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(framebufferSpec.Width, LIGHT_CULLING_TILE_SIZE),
            DivideToNextMultiple(framebufferSpec.Height, LIGHT_CULLING_TILE_SIZE));

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
            s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE, EAccessFlags::ACCESS_SHADER_READ);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE,
            EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::AtmosphericScatteringPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    if (s_RendererData->LightStruct->DirectionalLightCount != 0)
    {
        s_RendererData->GBuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        PushConstantBlock pc = {};
        pc.CameraDataBuffer  = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightDataBuffer   = s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->GetBDA();
        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->AtmospherePipeline);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->AtmospherePipeline, 0, 0,
                                                                                           sizeof(pc), &pc);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

        s_RendererData->GBuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    }
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::GeometryPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->GBuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));

    PushConstantBlock pc                  = {};
    pc.CameraDataBuffer                   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.LightDataBuffer                    = s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.VisiblePointLightIndicesDataBuffer = s_RendererData->PointLightIndicesSSBO->GetBDA();
    pc.VisibleSpotLightIndicesDataBuffer  = s_RendererData->SpotLightIndicesSSBO->GetBDA();
    pc.MaterialBufferIndex                = 0;
    pc.StorageImageIndex                  = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].m_Index;
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ForwardPlusOpaquePipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardPlusOpaquePipeline, 0, 0,
                                                                                       sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferOpaque, sizeof(uint32_t), s_RendererData->DrawBufferOpaque, 0,
        (s_RendererData->DrawBufferOpaque->GetSpecification().BufferCapacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand),
        sizeof(DrawMeshTasksIndirectCommand));

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));

    pc.MaterialBufferIndex = 1;
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->ForwardPlusTransparentPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->ForwardPlusTransparentPipeline, 0, 0,
                                                                                       sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferTransparent, sizeof(uint32_t), s_RendererData->DrawBufferTransparent, 0,
        (s_RendererData->DrawBufferTransparent->GetSpecification().BufferCapacity - sizeof(uint32_t)) /
            sizeof(DrawMeshTasksIndirectCommand),
        sizeof(DrawMeshTasksIndirectCommand));

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->GBuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::ScreenSpaceShadowsPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("ScreenSpaceShadowsPass", {.1f, .1f, .1f});

    s_RendererData->SSShadowsImage->ClearColor(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], glm::vec4(0.f));

    if (s_RendererData->bAnybodyCastsShadows)
    {
        PushConstantBlock pc                  = {};
        pc.CameraDataBuffer                   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightDataBuffer                    = s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.VisiblePointLightIndicesDataBuffer = s_RendererData->PointLightIndicesSSBO->GetBDA();
        pc.VisibleSpotLightIndicesDataBuffer  = s_RendererData->SpotLightIndicesSSBO->GetBDA();
        pc.StorageImageIndex                  = s_RendererData->SSShadowsImage->GetBindlessIndex();
        pc.AlbedoTextureIndex                 = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->SSShadowsPipeline);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->SSShadowsPipeline, 0, 0,
                                                                                           sizeof(pc), &pc);
        const auto& sssImageSpec = s_RendererData->SSShadowsImage->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(sssImageSpec.Width, SSS_LOCAL_GROUP_SIZE), DivideToNextMultiple(sssImageSpec.Height, SSS_LOCAL_GROUP_SIZE),
            1);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE,
            EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT, EAccessFlags::ACCESS_SHADER_READ);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BloomPass()
{
    if (IsWorldEmpty()) return;
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
    {
        s_RendererData->BloomFramebuffer[i]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        PushConstantBlock pc  = {};
        pc.AlbedoTextureIndex = (i == 0 ? s_RendererData->GBuffer->GetAttachments()[1].m_Index
                                        : s_RendererData->BloomFramebuffer.at(i - 1)->GetAttachments()[0].m_Index);
        pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->BloomPipeline[i]);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->BloomPipeline[i], 0, 0,
                                                                                           sizeof(pc), &pc);
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

        s_RendererData->BloomFramebuffer[i]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::RayTracingPass()
{
    if (IsWorldEmpty()) return;
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

#if 0
    PushConstantBlock pc    = {};
    pc.CameraDataBuffer     = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.StorageImageIndex    = s_RendererData->RTImage->GetBindlessIndex();
    const auto& rtImageSpec = s_RendererData->RTImage->GetSpecification();
#if 0

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->RTPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->RTPipeline, 0, 0, sizeof(pc), &pc);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->TraceRays(s_RendererData->RTSBT, rtImageSpec.Width, rtImageSpec.Height,
                                                                               1);
#else
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->RTComputePipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->RTComputePipeline, 0, 0, sizeof(pc),
                                                                                       &pc);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(DivideToNextMultiple(rtImageSpec.Width, 16),
                                                                              DivideToNextMultiple(rtImageSpec.Height, 16));

#endif
#endif

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::CompositePass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->CompositeFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], s_RendererData->CompositePipeline);

    PushConstantBlock pc  = {};
    pc.StorageImageIndex  = s_RendererData->GBuffer->GetAttachments()[0].m_Index;
    pc.AlbedoTextureIndex = s_RendererData->BloomFramebuffer[1]->GetAttachments()[0].m_Index;
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->CompositePipeline, 0, 0, sizeof(pc),
                                                                                       &pc);

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

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || dl.bCastShadows;

    s_RendererData->LightStruct->DirectionalLights[s_RendererData->LightStruct->DirectionalLightCount] = dl;
    ++s_RendererData->LightStruct->DirectionalLightCount;
}

void Renderer::AddPointLight(const PointLight& pl)
{
    if (s_RendererData->LightStruct->PointLightCount + 1 > MAX_POINT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max point light limit reached!");
        return;
    }

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || pl.bCastShadows;

    s_RendererData->LightStruct->PointLights[s_RendererData->LightStruct->PointLightCount] = pl;
    ++s_RendererData->LightStruct->PointLightCount;
}

void Renderer::AddSpotLight(const SpotLight& sl)
{
    if (s_RendererData->LightStruct->SpotLightCount + 1 > MAX_SPOT_LIGHTS)
    {
        LOG_TAG_WARN(RENDERER, "Max spot light limit reached!");
        return;
    }

    s_RendererData->bAnybodyCastsShadows = s_RendererData->bAnybodyCastsShadows || sl.bCastShadows;

    s_RendererData->LightStruct->SpotLights[s_RendererData->LightStruct->SpotLightCount] = sl;
    ++s_RendererData->LightStruct->SpotLightCount;
}

Shared<Image> Renderer::GetFinalPassImage()
{
    PFR_ASSERT(s_RendererData, "RendererData is not valid!");
    PFR_ASSERT(s_RendererData->CompositeFramebuffer && s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment,
               "Invalid composite framebuffer or its attachment!");
    return s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment;
}

const std::map<std::string, Shared<Image>> Renderer::GetRenderTargetList()
{
    PFR_ASSERT(s_RendererData, "RendererData is not valid!");
    std::map<std::string, Shared<Image>> renderTargetList;

    renderTargetList["GBuffer"]   = s_RendererData->GBuffer->GetAttachments()[0].Attachment;
    renderTargetList["SSAO"]      = s_RendererData->SSAO.Framebuffer->GetAttachments()[0].Attachment;
    renderTargetList["HBAO"]      = s_RendererData->HBAO.Framebuffer->GetAttachments()[0].Attachment;
    renderTargetList["BlurAO"]    = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].Attachment;
    renderTargetList["Heatmap"]   = s_RendererData->LightHeatMapImage;
    renderTargetList["Frustums"]  = s_RendererData->FrustumDebugImage;
    renderTargetList["RayTrace"]  = s_RendererData->RTImage;
    renderTargetList["SSShadows"] = s_RendererData->SSShadowsImage;

    return renderTargetList;
}

}  // namespace Pathfinder