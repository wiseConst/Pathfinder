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
                         {"ForwardPlus", std::map<std::string, std::string>{std::make_pair<std::string, std::string>(
                                             "PFR_RENDER_OPAQUE_EARLY_FRAGMENT_TESTS", "")}},
                         {"DepthPrePass"},
                         {"ObjectCulling"},
                         {"LightCulling"},
                         {"ComputeFrustums"},
                         {"Composite"},
                         {"AO/SSAO"},
                         {"AO/HBAO"},
                         {"Shadows/DirShadowMap"},
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
                          [](Shared<Buffer>& cameraSSBO)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                            EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              ubSpec.Data                = &s_RendererData->CameraStruct;
                              ubSpec.DataSize            = sizeof(s_RendererData->CameraStruct);

                              cameraSSBO = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->LightsSSBO,
                          [](Shared<Buffer>& lightsSSBO)
                          {
                              BufferSpecification lightsSSBOSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                                    EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS |
                                                                    EBufferUsage::BUFFER_USAGE_TRANSFER_DESTINATION};
                              lightsSSBOSpec.Data                = s_RendererData->LightStruct.get();
                              lightsSSBOSpec.DataSize            = sizeof(LightData);

                              lightsSSBO = Buffer::Create(lightsSSBOSpec);
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

    GraphicsPipelineOptions depthGraphicsPipelineOptions = {};
    depthGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
    depthGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->DepthPrePassFramebuffer;
    depthGraphicsPipelineOptions.bMeshShading            = true;
    depthGraphicsPipelineOptions.bDepthTest              = true;
    depthGraphicsPipelineOptions.bDepthWrite             = true;
    depthGraphicsPipelineOptions.DepthCompareOp          = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    depthPrePassPipelineSpec.PipelineOptions             = std::make_optional<GraphicsPipelineOptions>(depthGraphicsPipelineOptions);
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
        forwardPipelineSpec.Shader                = ShaderLibrary::Get(
            {"ForwardPlus",
                            std::map<std::string, std::string>{std::make_pair<std::string, std::string>("PFR_RENDER_OPAQUE_EARLY_FRAGMENT_TESTS", "")}});
        // Rendering view normals useless for now.
        // forwardPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        forwardPipelineSpec.bBindlessCompatible = true;

        GraphicsPipelineOptions forwardGraphicsPipelineOptions = {};

        forwardGraphicsPipelineOptions.CullMode          = ECullMode::CULL_MODE_BACK;
        forwardGraphicsPipelineOptions.bDepthTest        = true;
        forwardGraphicsPipelineOptions.bDepthWrite       = false;
        forwardGraphicsPipelineOptions.DepthCompareOp    = ECompareOp::COMPARE_OP_EQUAL;
        forwardGraphicsPipelineOptions.TargetFramebuffer = s_RendererData->GBuffer;
        forwardGraphicsPipelineOptions.bMeshShading      = true;
        forwardGraphicsPipelineOptions.bBlendEnable      = true;
        forwardGraphicsPipelineOptions.BlendMode         = EBlendMode::BLEND_MODE_ALPHA;
        forwardPipelineSpec.PipelineOptions              = std::make_optional<GraphicsPipelineOptions>(forwardGraphicsPipelineOptions);

        PipelineBuilder::Push(s_RendererData->ForwardPlusOpaquePipeline, forwardPipelineSpec);

        forwardPipelineSpec.Shader    = ShaderLibrary::Get("ForwardPlus");
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

        GraphicsPipelineOptions compositeGraphicsPipelineOptions = {};
        compositeGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
        compositeGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->CompositeFramebuffer;
        compositePipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(compositeGraphicsPipelineOptions);

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
            s_RendererData->bNeedsFrustumsRecomputing = true;
            s_RendererData->FrustumDebugImage->Resize(width, height);

            s_RendererData->LightHeatMapImage->Resize(width, height);

            s_RendererData->CascadeDebugImage->Resize(width, height);

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
        GraphicsPipelineOptions ssaoGraphicsPipelineOptions = {};
        ssaoGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
        ssaoGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->SSAO.Framebuffer;
        ssaoPipelineSpec.PipelineOptions                    = std::make_optional<GraphicsPipelineOptions>(ssaoGraphicsPipelineOptions);

        PipelineBuilder::Push(s_RendererData->SSAO.Pipeline, ssaoPipelineSpec);
    }

    // Noise texture for ambient occlusions
    {
        std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
        std::mt19937_64 rndEngine;
#define SSAO_NOISE_DIM 4
        std::vector<glm::vec4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
        for (uint32_t i{}; i < SSAO_NOISE_DIM * SSAO_NOISE_DIM; ++i)
        {
            ssaoNoise[i] = glm::vec4(randomFloats(rndEngine) * 2.0f - 1.0f, randomFloats(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
        }

        TextureSpecification aoNoiseTextureSpec = {4, 4};
        aoNoiseTextureSpec.bBindlessUsage       = true;
        aoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_NEAREST;
        aoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
        aoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA32F;
        s_RendererData->AONoiseTexture = Texture2D::Create(aoNoiseTextureSpec, ssaoNoise.data(), ssaoNoise.size() * sizeof(ssaoNoise[0]));
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

        GraphicsPipelineOptions hbaoGraphicsPipelineOptions = {};
        hbaoGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
        hbaoGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->HBAO.Framebuffer;
        hbaoPipelineSpec.PipelineOptions                    = std::make_optional<GraphicsPipelineOptions>(hbaoGraphicsPipelineOptions);

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

            GraphicsPipelineOptions blurAOGraphicsPipelineOptions = {};
            blurAOGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
            blurAOGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->BlurAOFramebuffer[i];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGraphicsPipelineOptions);

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

            GraphicsPipelineOptions blurAOGraphicsPipelineOptions = {};
            blurAOGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
            blurAOGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->BlurAOFramebuffer[1];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGraphicsPipelineOptions);

            PipelineBuilder::Push(s_RendererData->MedianBlurAOPipeline, blurAOPipelineSpec);
        }

        {
            PipelineSpecification blurAOPipelineSpec = {"BoxBlurAO"};
            blurAOPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            blurAOPipelineSpec.Shader                = ShaderLibrary::Get("Post/BoxBlur");
            blurAOPipelineSpec.bBindlessCompatible   = true;

            GraphicsPipelineOptions blurAOGraphicsPipelineOptions = {};
            blurAOGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
            blurAOGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->BlurAOFramebuffer[1];
            blurAOPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(blurAOGraphicsPipelineOptions);

            PipelineBuilder::Push(s_RendererData->BoxBlurAOPipeline, blurAOPipelineSpec);
        }
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
                s_RendererData->LightStruct = MakeUnique<LightData>();
            });

        PFR_ASSERT(!s_RendererData->DirShadowMaps.empty() && MAX_DIR_LIGHTS > 0, "Failed to create directional shadow map framebuffers!");
        PipelineSpecification dirShadowMapPipelineSpec = {"DirShadowMap"};
        dirShadowMapPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        dirShadowMapPipelineSpec.Shader                = ShaderLibrary::Get("Shadows/DirShadowMap");
        dirShadowMapPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions dirShadowMapGraphicsPipelineOptions = {};
        dirShadowMapGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->DirShadowMaps[0];
        dirShadowMapGraphicsPipelineOptions.bMeshShading            = true;
        dirShadowMapGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_FRONT;
        dirShadowMapGraphicsPipelineOptions.bDepthTest              = true;
        dirShadowMapGraphicsPipelineOptions.bDepthWrite             = true;
        dirShadowMapGraphicsPipelineOptions.DepthCompareOp          = ECompareOp::COMPARE_OP_LESS_OR_EQUAL;
        dirShadowMapPipelineSpec.PipelineOptions = std::make_optional<GraphicsPipelineOptions>(dirShadowMapGraphicsPipelineOptions);

        PipelineBuilder::Push(s_RendererData->DirShadowMapPipeline, dirShadowMapPipelineSpec);
    }

    {
        PipelineSpecification asPipelineSpec = {"AtmosphericScattering"};
        asPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        asPipelineSpec.Shader                = ShaderLibrary::Get("AtmosphericScattering");
        asPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions asGraphicsPipelineOptions = {};
        asGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->GBuffer;
        asGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
        asPipelineSpec.PipelineOptions                    = std::make_optional<GraphicsPipelineOptions>(asGraphicsPipelineOptions);
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

    {
        std::vector<Shared<Image>> attachments(s_RendererData->DirShadowMaps.size());
        for (size_t i{}; i < attachments.size(); ++i)
            attachments[i] = s_RendererData->DirShadowMaps[i]->GetDepthAttachment();

        // TODO: Remove duplicate Shader->Sets() since opaque and transparent pipelines tied to only one shader.(but shouldn't, each
        // pipeline should have its own shader)
        s_RendererData->ForwardPlusOpaquePipeline->GetSpecification().Shader->Set("u_DirShadowmap", attachments);
        s_RendererData->ForwardPlusTransparentPipeline->GetSpecification().Shader->Set("u_DirShadowmap", attachments);
    }

    s_RendererData->RenderGraph = RenderGraphBuilder::Create("RenderGraphDescription.pfr");

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

    Timer t = {};
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
        s_RendererData->PassStats["DepthPrePass"]              = renderTimestamps[1];
        s_RendererData->PassStats["DirShadowMapPass"]          = renderTimestamps[2];
        s_RendererData->PassStats["AtmosphericScatteringPass"] = renderTimestamps[3];
        s_RendererData->PassStats["SSAOPass"]                  = renderTimestamps[4];
        s_RendererData->PassStats["HBAOPass"]                  = renderTimestamps[5];
        s_RendererData->PassStats["BlurAOPass"]                = renderTimestamps[6];
        s_RendererData->PassStats["ComputeFrustumsPass"]       = renderTimestamps[7];
        s_RendererData->PassStats["LightCullingPass"]          = renderTimestamps[8];
        s_RendererData->PassStats["GeometryPass"]              = renderTimestamps[9];
        s_RendererData->PassStats["RayTracingPass"]            = renderTimestamps[10];
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

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginRecording(true);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginPipelineStatisticsQuery();

    Renderer2D::Begin();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);

    GraphicsContext::Get().FillMemoryBudgetStats(s_RendererStats.MemoryBudgets);
    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
}

void Renderer::Flush(const Unique<UILayer>& uiLayer)
{
    Timer t = {};
    s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->SetData(s_RendererData->LightStruct.get(), sizeof(LightData));

    s_BindlessRenderer->UpdateDataIfNeeded();


    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    ObjectCullingPass();

   /* s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                              EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);*/
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ);
 RayTracingPass();

    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->GBuffer->Clear(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    DepthPrePass();
    DirShadowMapPass();

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
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    ComputeFrustumsPass();
    LightCullingPass();

    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
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

    s_RendererData->LightStruct->CascadePlaneDistances[0] = camera.GetFarPlaneDepth() / 400.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[1] = camera.GetFarPlaneDepth() / 200.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[2] = camera.GetFarPlaneDepth() / 80.0f;
    s_RendererData->LightStruct->CascadePlaneDistances[3] = camera.GetFarPlaneDepth() / 15.0f;

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
    pc.pad0.x                             = 25.0f;
    pc.AlbedoTextureIndex                 = s_RendererData->CascadeDebugImage->GetBindlessIndex();
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

void Renderer::DirShadowMapPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    if (s_RendererData->LightStruct->DirectionalLightCount != 0)
    {
        auto renderMeshFunc = [](const RenderObject& renderObject, const uint32_t directionalLightIndex)
        {
            PushConstantBlock pc = {renderObject.Transform};
            pc.StorageImageIndex = directionalLightIndex;  // Index into matrices projection array
            pc.CameraDataBuffer  = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(s_RendererData->DirShadowMapPipeline, 0, 0,
                                                                                               sizeof(pc), &pc);

            const uint32_t meshletCount = renderObject.submesh->GetMeshletBuffer()->GetSpecification().BufferCapacity / sizeof(Meshlet);
            const uint32_t groupCountX  = DivideToNextMultiple(meshletCount, MESHLET_LOCAL_GROUP_SIZE);
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasks(groupCountX, 1, 1);

            s_RendererStats.TriangleCount +=
                renderObject.submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) / 3;
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

    s_RendererData->LightStruct->SpotLights[s_RendererData->LightStruct->SpotLightCount++] = sl;
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

    renderTargetList["GBuffer"]  = s_RendererData->GBuffer->GetAttachments()[0].Attachment;
    renderTargetList["SSAO"]     = s_RendererData->SSAO.Framebuffer->GetAttachments()[0].Attachment;
    renderTargetList["HBAO"]     = s_RendererData->HBAO.Framebuffer->GetAttachments()[0].Attachment;
    renderTargetList["BlurAO"]   = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].Attachment;
    renderTargetList["Heatmap"]  = s_RendererData->LightHeatMapImage;
    renderTargetList["Frustums"] = s_RendererData->FrustumDebugImage;
    renderTargetList["RayTrace"] = s_RendererData->RTImage;

    return renderTargetList;
}

}  // namespace Pathfinder