#include <PathfinderPCH.h>
#include "Renderer.h"
#include "Renderer2D.h"

#include <Core/Application.h>
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
#include "Debug/DebugRenderer.h"

#include "RenderGraph/RenderGraph.h"

namespace Pathfinder
{

Renderer::Renderer()  = default;
Renderer::~Renderer() = default;

void Renderer::Init()
{
    s_RendererData = MakeUnique<RendererData>();
    ShaderLibrary::Init();
    PipelineLibrary::Init();
    s_BindlessRenderer = BindlessRenderer::Create();
    SamplerStorage::Init();
    RayTracingBuilder::Init();

    s_RendererSettings.bVSync   = Application::Get().GetWindow()->IsVSync();
    s_RendererSettings.BlurType = EBlurType::BLUR_TYPE_BOX;
    s_RendererSettings.AOType   = EAmbientOcclusionType::AMBIENT_OCCLUSION_TYPE_SSAO;
    s_RendererData->LightStruct = MakeUnique<LightData>();

    ShaderLibrary::Load({{"DepthPrePass"},
                         {"SSShadows"},
                         {"ObjectCulling", {{"__PFR_RENDER_OPAQUE_OBJECTS_", ""}}},
                         {"ObjectCulling", {{"__PFR_RENDER_TRANSPARENT_OBJECTS_", ""}}},
                         {"ForwardPlus", {{"__PFR_RENDER_OPAQUE_OBJECTS_", ""}}},
                         {"ForwardPlus", {{"__PFR_RENDER_TRANSPARENT_OBJECTS_", ""}}},
                         {"LightCulling", {{"ADVANCED_CULLING", ""}}},
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
                              uploadHeapSpec.Capacity            = s_RendererData->s_MAX_UPLOAD_HEAP_CAPACITY;

                              uploadHeap = Buffer::Create(uploadHeapSpec);
                          });

    std::ranges::for_each(s_RendererData->CameraSSBO,
                          [](auto& cameraSSBO)
                          {
                              BufferSpecification ubSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                            EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              ubSpec.Capacity            = sizeof(s_RendererData->CameraStruct);

                              cameraSSBO = Buffer::Create(ubSpec);
                          });

    std::ranges::for_each(s_RendererData->LightsSSBO,
                          [](auto& lightsSSBO)
                          {
                              BufferSpecification lightsSSBOSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                                    EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              lightsSSBOSpec.Capacity            = sizeof(LightData);
                              lightsSSBO                         = Buffer::Create(lightsSSBOSpec);
                          });
    {
        CommandBufferSpecification cbSpec = {ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS,
                                             ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY};
        cbSpec.ThreadID                   = ThreadPool::MapThreadID(ThreadPool::GetMainThreadID());

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
        BufferSpecification bufferSpec   = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                            EBufferUsage::BUFFER_USAGE_INDIRECT | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        bufferSpec.Capacity              = sizeof(uint32_t) + 1 * sizeof(DrawMeshTasksIndirectCommand);
        bufferSpec.bMapPersistent        = true;
        s_RendererData->DrawBufferOpaque = Buffer::Create(bufferSpec);
    }

    {
        BufferSpecification bufferSpec   = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_DATA_OPAQUE_BINDING, true};
        bufferSpec.Capacity              = 1 * sizeof(MeshData);
        s_RendererData->MeshesDataOpaque = Buffer::Create(bufferSpec);
    }

    {
        BufferSpecification bufferSpec           = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        bufferSpec.Capacity                      = 1 * sizeof(uint32_t);
        s_RendererData->CulledMeshesBufferOpaque = Buffer::Create(bufferSpec);
    }

    {
        BufferSpecification bufferSpec        = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                                 EBufferUsage::BUFFER_USAGE_INDIRECT | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        bufferSpec.Capacity                   = sizeof(uint32_t) + 1 * sizeof(DrawMeshTasksIndirectCommand);
        bufferSpec.bMapPersistent             = true;
        s_RendererData->DrawBufferTransparent = Buffer::Create(bufferSpec);
    }

    {
        BufferSpecification bufferSpec        = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_DATA_TRANSPARENT_BINDING, true};
        bufferSpec.Capacity                   = 1 * sizeof(MeshData);
        s_RendererData->MeshesDataTransparent = Buffer::Create(bufferSpec);
    }

    {
        BufferSpecification bufferSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        bufferSpec.Capacity            = 1 * sizeof(uint32_t);
        s_RendererData->CulledMeshesBufferTransparent = Buffer::Create(bufferSpec);
    }

    {
        FramebufferSpecification framebufferSpec = {"DepthPrePass"};

        FramebufferAttachmentSpecification depthAttachmentSpec = {EImageFormat::FORMAT_D32F,
                                                                  EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                  EOp::CLEAR,
                                                                  EOp::STORE,
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

    ShaderLibrary::WaitUntilShadersLoaded();

    {
        PipelineSpecification objectCullingPipelineSpec = {"ObjectCullingOpaque"};
        objectCullingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        objectCullingPipelineSpec.PipelineOptions       = MakeOptional<ComputePipelineOptions>();
        objectCullingPipelineSpec.Shader                = ShaderLibrary::Get({
            "ObjectCulling",
            {{"__PFR_RENDER_OPAQUE_OBJECTS_", ""}},
        });
        objectCullingPipelineSpec.bBindlessCompatible   = true;
        s_RendererData->OpaqueObjectCullingPipelineHash = PipelineLibrary::Push(objectCullingPipelineSpec);

        objectCullingPipelineSpec.Shader                     = ShaderLibrary::Get({
            "ObjectCulling",
            {{"__PFR_RENDER_TRANSPARENT_OBJECTS_", ""}},
        });
        objectCullingPipelineSpec.DebugName                  = "ObjectCullingTransparent";
        s_RendererData->TransparentObjectCullingPipelineHash = PipelineLibrary::Push(objectCullingPipelineSpec);
    }

    {
#if 0
        {
            PipelineSpecification rtPipelineSpec = {"RayTracing"};
            rtPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_RAY_TRACING;
            rtPipelineSpec.bBindlessCompatible   = true;
            rtPipelineSpec.Shader                = ShaderLibrary::Get("RayTrace");

            RayTracingPipelineOptions rtPipelineOptions = {};
            rtPipelineSpec.PipelineOptions              = MakeOptional<RayTracingPipelineOptions>(rtPipelineOptions);

            s_RendererData->RTPipelineHash = Pipeline::Create(rtPipelineSpec);
        }
        s_RendererData->RTSBT = ShaderLibrary::Get("RayTrace")->CreateSBT(s_RendererData->RTPipeline);

        {
            PipelineSpecification rtComputePipelineSpec = {"RayTracingCompute"};
            rtComputePipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
            rtComputePipelineSpec.bBindlessCompatible   = true;
            rtComputePipelineSpec.Shader                = ShaderLibrary::Get("RayTrace");

            ComputePipelineOptions rtComputePipelineOptions = {};
            rtComputePipelineSpec.PipelineOptions           = MakeOptional<ComputePipelineOptions>(rtComputePipelineOptions);

            PipelineLibrary::Push(s_RendererData->RTComputePipeline, rtComputePipelineSpec);
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
    depthPrePassPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(depthGPO);
    s_RendererData->DepthPrePassPipelineHash = PipelineLibrary::Push(depthPrePassPipelineSpec);

    {
        FramebufferSpecification framebufferSpec = {"GBuffer"};

        // Albedo
        {
            FramebufferAttachmentSpecification attachmentSpec = {EImageFormat::FORMAT_RGBA16F,
                                                                 EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 EOp::LOAD,
                                                                 EOp::STORE,
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
                                                                 EOp::LOAD,
                                                                 EOp::STORE,
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
                                                                 EOp::LOAD,
                                                                 EOp::STORE,
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
            attachmentSpec.LoadOp                             = EOp::LOAD;
            attachmentSpec.StoreOp                            = EOp::STORE;
            framebufferSpec.Attachments.emplace_back(attachmentSpec);
        }

        s_RendererData->GBuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->GBuffer->Resize(width, height); });
    }

    {
        PipelineSpecification forwardPipelineSpec = {"ForwardPlusOpaque"};
        forwardPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        forwardPipelineSpec.Shader                = ShaderLibrary::Get({
            "ForwardPlus",
            {{"__PFR_RENDER_OPAQUE_OBJECTS_", ""}},
        });
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
        forwardPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(forwardGPO);

        s_RendererData->ForwardPlusOpaquePipelineHash = PipelineLibrary::Push(forwardPipelineSpec);

        forwardPipelineSpec.DebugName = "ForwardPlusTransparent";
        forwardPipelineSpec.Shader    = ShaderLibrary::Get({
            "ForwardPlus",
            {{"__PFR_RENDER_TRANSPARENT_OBJECTS_", ""}},
        });
        std::get<GraphicsPipelineOptions>(forwardPipelineSpec.PipelineOptions.value()).DepthCompareOp =
            std::get<GraphicsPipelineOptions>(depthPrePassPipelineSpec.PipelineOptions.value()).DepthCompareOp;
        s_RendererData->ForwardPlusTransparentPipelineHash = PipelineLibrary::Push(forwardPipelineSpec);
    }

    {
        FramebufferSpecification framebufferSpec                         = {"Composite"};
        const FramebufferAttachmentSpecification compositeAttachmentSpec = {EImageFormat::FORMAT_A2R10G10B10_UNORM_PACK32,
                                                                            EImageLayout::IMAGE_LAYOUT_UNDEFINED,
                                                                            EOp::CLEAR,
                                                                            EOp::STORE,
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
        compositePipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(compositeGPO);

        s_RendererData->CompositePipelineHash = PipelineLibrary::Push(compositePipelineSpec);
    }

    Renderer2D::Init();
#if PFR_DEBUG
    DebugRenderer::Init();
#endif

    // Light-Culling
    {
        PipelineSpecification lightCullingPipelineSpec = {"LightCulling"};
        lightCullingPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        lightCullingPipelineSpec.Shader                = ShaderLibrary::Get({"LightCulling", {{"ADVANCED_CULLING", ""}}});
        lightCullingPipelineSpec.bBindlessCompatible   = true;
        lightCullingPipelineSpec.PipelineOptions       = MakeOptional<ComputePipelineOptions>();
        s_RendererData->LightCullingPipelineHash       = PipelineLibrary::Push(lightCullingPipelineSpec);

        PipelineSpecification computeFrustumsPipelineSpec = {"ComputeFrustums"};
        computeFrustumsPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_COMPUTE;
        computeFrustumsPipelineSpec.Shader                = ShaderLibrary::Get("ComputeFrustums");
        computeFrustumsPipelineSpec.bBindlessCompatible   = true;
        computeFrustumsPipelineSpec.PipelineOptions       = MakeOptional<ComputePipelineOptions>();
        s_RendererData->ComputeFrustumsPipelineHash       = PipelineLibrary::Push(computeFrustumsPipelineSpec);
    }

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize = MAX_POINT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * width * height;
        sbSpec.Capacity     = sbSize;

        s_RendererData->PointLightIndicesSSBO = Buffer::Create(sbSpec);
    }

    {
        BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize = MAX_SPOT_LIGHTS * sizeof(LIGHT_INDEX_TYPE) * width * height;
        sbSpec.Capacity     = sbSize;

        s_RendererData->SpotLightIndicesSSBO = Buffer::Create(sbSpec);
    }

    {
        BufferSpecification frustumsSBSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};

        const uint32_t width =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Width, LIGHT_CULLING_TILE_SIZE);
        const uint32_t height =
            DivideToNextMultiple(s_RendererData->DepthPrePassFramebuffer->GetSpecification().Height, LIGHT_CULLING_TILE_SIZE);
        const size_t sbSize     = sizeof(TileFrustum) * width * height;
        frustumsSBSpec.Capacity = sbSize;

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
        FramebufferSpecification framebufferSpec                  = {"AO"};
        const FramebufferAttachmentSpecification aoAttachmentSpec = {EImageFormat::FORMAT_R8_UNORM,
                                                                     EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                     EOp::CLEAR,
                                                                     EOp::STORE,
                                                                     glm::vec4(1.0f, glm::vec3(0.0f)),
                                                                     false,
                                                                     ESamplerWrap::SAMPLER_WRAP_REPEAT,
                                                                     ESamplerFilter::SAMPLER_FILTER_LINEAR,
                                                                     true};

        framebufferSpec.Attachments.emplace_back(aoAttachmentSpec);
        s_RendererData->AOFramebuffer = Framebuffer::Create(framebufferSpec);

        Application::Get().GetWindow()->AddResizeCallback([](uint32_t width, uint32_t height)
                                                          { s_RendererData->AOFramebuffer->Resize(width, height); });

        PipelineSpecification ssaoPipelineSpec = {"SSAO"};
        ssaoPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        ssaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/SSAO");
        ssaoPipelineSpec.bBindlessCompatible   = true;

        // Gbuffer goes as last, so there's no way for SSAO to get it.
        // ssaoPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(true);
        GraphicsPipelineOptions ssaoGPO  = {};
        ssaoGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        ssaoGPO.TargetFramebuffer        = s_RendererData->AOFramebuffer;
        ssaoPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(ssaoGPO);

        s_RendererData->SSAOPipelineHash = PipelineLibrary::Push(ssaoPipelineSpec);
    }

    // Noise texture for ambient occlusions
    {
        const auto& appSpec       = Application::Get().GetSpecification();
        const auto rotTexturePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / "Other/rot_texture.bmp";
        int32_t x = 0, y = 0, channels = 0;
        uint8_t* rawImageData = reinterpret_cast<uint8_t*>(ImageUtils::LoadRawImage(rotTexturePath, false, &x, &y, &channels));
        void* rgbaImageData   = ImageUtils::ConvertRgbToRgba(rawImageData, x, y);

        TextureSpecification aoNoiseTextureSpec = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
        aoNoiseTextureSpec.Format               = EImageFormat::FORMAT_RGBA32F;
        aoNoiseTextureSpec.bBindlessUsage       = true;
        aoNoiseTextureSpec.Filter               = ESamplerFilter::SAMPLER_FILTER_NEAREST;
        aoNoiseTextureSpec.Wrap                 = ESamplerWrap::SAMPLER_WRAP_REPEAT;
        s_RendererData->AONoiseTexture          = Texture2D::Create(aoNoiseTextureSpec, rgbaImageData, x * y * sizeof(glm::vec4));

        free(rgbaImageData);
        ImageUtils::UnloadRawImage(rawImageData);
    }

    {

        PipelineSpecification hbaoPipelineSpec = {"HBAO"};
        hbaoPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        hbaoPipelineSpec.Shader                = ShaderLibrary::Get("AO/HBAO");
        hbaoPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions hbaoGPO  = {};
        hbaoGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
        hbaoGPO.TargetFramebuffer        = s_RendererData->AOFramebuffer;
        hbaoPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(hbaoGPO);

        s_RendererData->HBAOPipelineHash = PipelineLibrary::Push(hbaoPipelineSpec);
    }

    {
        // Ping-pong framebuffers
        for (uint32_t i{}; i < s_RendererData->BlurAOFramebuffer.size(); ++i)
        {
            const std::string blurTypeStr            = (i == 0) ? "Horiz" : "Vert";
            FramebufferSpecification framebufferSpec = {"BlurAOFramebuffer" + blurTypeStr};
            auto& bloomAttachmentSpec                = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_R8_UNORM, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, EOp::CLEAR, EOp::STORE,
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
            blurAOPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(blurAOGPO);

            s_RendererData->BlurAOPipelineHash[i] = PipelineLibrary::Push(blurAOPipelineSpec);
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
            blurAOPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(blurAOGPO);

            s_RendererData->MedianBlurAOPipelineHash = PipelineLibrary::Push(blurAOPipelineSpec);
        }

        {
            PipelineSpecification blurAOPipelineSpec = {"BoxBlurAO"};
            blurAOPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            blurAOPipelineSpec.Shader                = ShaderLibrary::Get("Post/BoxBlur");
            blurAOPipelineSpec.bBindlessCompatible   = true;

            GraphicsPipelineOptions blurAOGPO  = {};
            blurAOGPO.CullMode                 = ECullMode::CULL_MODE_BACK;
            blurAOGPO.TargetFramebuffer        = s_RendererData->BlurAOFramebuffer[1];
            blurAOPipelineSpec.PipelineOptions = MakeOptional<GraphicsPipelineOptions>(blurAOGPO);

            s_RendererData->BoxBlurAOPipelineHash = PipelineLibrary::Push(blurAOPipelineSpec);
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
        sssPipelineSpec.PipelineOptions       = MakeOptional<ComputePipelineOptions>();
        s_RendererData->SSShadowsPipelineHash = PipelineLibrary::Push(sssPipelineSpec);
    }

    {
        PipelineSpecification asPipelineSpec = {"AtmosphericScattering"};
        asPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
        asPipelineSpec.Shader                = ShaderLibrary::Get("AtmosphericScattering");
        asPipelineSpec.bBindlessCompatible   = true;

        GraphicsPipelineOptions asGPO          = {};
        asGPO.TargetFramebuffer                = s_RendererData->GBuffer;
        asGPO.CullMode                         = ECullMode::CULL_MODE_BACK;
        asPipelineSpec.PipelineOptions         = MakeOptional<GraphicsPipelineOptions>(asGPO);
        s_RendererData->AtmospherePipelineHash = PipelineLibrary::Push(asPipelineSpec);
    }

    {
        // Ping-pong framebuffers
        for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
        {
            const std::string blurTypeStr            = (i == 0) ? "Horiz" : "Vert";
            FramebufferSpecification framebufferSpec = {"BloomFramebuffer" + blurTypeStr};
            auto& bloomAttachmentSpec                = framebufferSpec.Attachments.emplace_back(
                EImageFormat::FORMAT_RGBA16F, EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, EOp::CLEAR, EOp::STORE, glm::vec4(0.0f),
                false, ESamplerWrap::SAMPLER_WRAP_REPEAT, ESamplerFilter::SAMPLER_FILTER_LINEAR, true);
            s_RendererData->BloomFramebuffer[i] = Framebuffer::Create(framebufferSpec);

            PipelineSpecification bloomPipelineSpec = {"Bloom" + blurTypeStr};
            bloomPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
            bloomPipelineSpec.Shader                = ShaderLibrary::Get("Post/GaussianBlur");
            bloomPipelineSpec.ShaderConstantsMap[EShaderStage::SHADER_STAGE_FRAGMENT].emplace_back(i);
            bloomPipelineSpec.bBindlessCompatible = true;

            GraphicsPipelineOptions bloomGraphicsPipelineOptions = {};
            bloomGraphicsPipelineOptions.TargetFramebuffer       = s_RendererData->BloomFramebuffer[i];
            bloomGraphicsPipelineOptions.CullMode                = ECullMode::CULL_MODE_BACK;
            bloomPipelineSpec.PipelineOptions                    = MakeOptional<GraphicsPipelineOptions>(bloomGraphicsPipelineOptions);

            s_RendererData->BloomPipelineHash[i] = PipelineLibrary::Push(bloomPipelineSpec);
        }

        Application::Get().GetWindow()->AddResizeCallback(
            [](uint32_t width, uint32_t height) {
                std::ranges::for_each(s_RendererData->BloomFramebuffer,
                                      [&](const auto& framebuffer) { framebuffer->Resize(width, height); });
            });
    }

    PipelineLibrary::Compile();
    LOG_TRACE("Renderer3D created!");

    s_RendererData->RenderGraph = MakeUnique<RenderGraph>(s_ENGINE_NAME);
    auto& objectCullingPass     = s_RendererData->RenderGraph->AddPass("ObjectCullingPass", EPassType::PASS_TYPE_COMPUTE, []() {});
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

    s_RendererData.reset();
    s_BindlessRenderer.reset();

    SamplerStorage::Shutdown();
    LOG_TRACE("Renderer3D destroyed!");
}

void Renderer::Begin()
{
    ShaderLibrary::DestroyGarbageIfNeeded();

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
    uint32_t prevImageViewCount         = s_RendererStats.ImageViewCount;
    s_RendererStats                     = {};
    s_RendererStats.DescriptorPoolCount = prevPoolCount;
    s_RendererStats.DescriptorSetCount  = prevDescriptorSetCount;
    s_RendererStats.ImageViewCount      = prevImageViewCount;

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
        switch (s_RendererSettings.AOType)
        {
            case EAmbientOcclusionType::AMBIENT_OCCLUSION_TYPE_SSAO: s_RendererData->PassStats["SSAOPass"] = renderTimestamps[5]; break;
            case EAmbientOcclusionType::AMBIENT_OCCLUSION_TYPE_HBAO: s_RendererData->PassStats["HBAOPass"] = renderTimestamps[5]; break;
        }
        s_RendererData->PassStats["BlurAOPass"]          = renderTimestamps[6];
        s_RendererData->PassStats["ComputeFrustumsPass"] = renderTimestamps[7];
        s_RendererData->PassStats["LightCullingPass"]    = renderTimestamps[8];
        s_RendererData->PassStats["GeometryPass"]        = renderTimestamps[9];
        s_RendererData->PassStats["2D-QuadPass"]         = renderTimestamps[10];
        s_RendererData->PassStats["BloomPass"]           = renderTimestamps[11];
        s_RendererData->PassStats["DebugPass"]           = renderTimestamps[11];
        s_RendererData->PassStats["CompositePass"]       = renderTimestamps[13];

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

    // NOTE: Once per frame
    // Bind mega descriptor set to render command buffer for Graphics, Compute, RT stages.
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    s_BindlessRenderer->Bind(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);

    // Bind mega descriptor set to compute command buffer for Graphics, Compute, RT stages.
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]);
    s_BindlessRenderer->Bind(s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex],
                             EPipelineStage::PIPELINE_STAGE_RAY_TRACING_SHADER_BIT);

    Renderer2D::Begin();

    GraphicsContext::Get().FillMemoryBudgetStats(s_RendererStats.MemoryBudgets);
    s_RendererStats.RHITime += t.GetElapsedMilliseconds();
}

void Renderer::Flush(const Unique<UILayer>& uiLayer)
{
    Timer t = {};

    s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->SetData(s_RendererData->LightStruct.get(), sizeof(LightData));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT | PIPELINE_STAGE_FRAGMENT_SHADER_BIT | PIPELINE_STAGE_RAY_TRACING_SHADER_BIT,
        EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_BindlessRenderer->UpdateDataIfNeeded();

    ObjectCullingPass();
    RayTracingPass();

    s_RendererData->GBuffer->Clear(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    DepthPrePass();
    ScreenSpaceShadowsPass();

    AtmosphericScatteringPass();

    switch (s_RendererSettings.AOType)
    {
        case EAmbientOcclusionType::AMBIENT_OCCLUSION_TYPE_SSAO: SSAOPass(); break;
        case EAmbientOcclusionType::AMBIENT_OCCLUSION_TYPE_HBAO: HBAOPass(); break;
    }

    BlurAOPass();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ_BIT);
    ComputeFrustumsPass();
    LightCullingPass();

    GeometryPass();

    Renderer2D::Flush(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    BloomPass();

#if PFR_DEBUG
    DebugRenderer::Flush(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
#endif

    CompositePass();

    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    const auto transferSyncPoint = s_RendererData->TransferCommandBuffer[s_RendererData->FrameIndex]->Submit();
    const auto computeSyncPoint  = s_RendererData->ComputeCommandBuffer[s_RendererData->FrameIndex]->Submit({transferSyncPoint});

    Application::Get().GetWindow()->CopyToWindow(s_RendererData->CompositeFramebuffer->GetAttachments()[0].Attachment);

    if (uiLayer) uiLayer->EndRender();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndPipelineStatisticsQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndRecording();

    const auto& swapchain = Application::Get().GetWindow()->GetSwapchain();
    const auto swapchainImageAvailableSyncPoint =
        SyncPoint::Create(swapchain->GetImageAvailableSemaphore(), 1, EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    const auto swapchainRenderFinishedSyncPoint =
        SyncPoint::Create(swapchain->GetRenderSemaphore(), 1, EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Submit(
        {transferSyncPoint, computeSyncPoint, swapchainImageAvailableSyncPoint}, {swapchainRenderFinishedSyncPoint},
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
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CameraSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT | PIPELINE_STAGE_FRAGMENT_SHADER_BIT | PIPELINE_STAGE_RAY_TRACING_SHADER_BIT,
        EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);
}

void Renderer::EndScene() {}

void Renderer::BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline> pipeline)
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

void Renderer::DepthPrePass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->DepthPrePassFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("OPAQUE", glm::vec3(0.2f, 0.5f, 0.9f));

    PushConstantBlock pc = {};
    pc.CameraDataBuffer  = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.addr0             = s_RendererData->CulledMeshesBufferOpaque->GetBDA();
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                 PipelineLibrary::Get(s_RendererData->DepthPrePassPipelineHash));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->DepthPrePassPipelineHash), 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferOpaque, sizeof(uint32_t), s_RendererData->DrawBufferOpaque, 0,
        (s_RendererData->DrawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand),
        sizeof(DrawMeshTasksIndirectCommand));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->DepthPrePassFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::ObjectCullingPass()
{
    if (IsWorldEmpty()) return;

    // 1. Sort them front to back, to minimize overdraw.
    std::sort(std::execution::par, s_RendererData->OpaqueObjects.begin(), s_RendererData->OpaqueObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const float lhsDist = glm::length(lhs.Translation - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhs.Translation - s_RendererData->CameraStruct.Position);

                  return lhsDist < rhsDist;  // Front to back drawing(minimize overdraw)
              });

    // 1. Sort them back to front.
    std::sort(std::execution::par, s_RendererData->TransparentObjects.begin(), s_RendererData->TransparentObjects.end(),
              [](const auto& lhs, const auto& rhs) -> bool
              {
                  const float lhsDist = glm::length(lhs.Translation - s_RendererData->CameraStruct.Position);
                  const float rhsDist = glm::length(rhs.Translation - s_RendererData->CameraStruct.Position);

                  return lhsDist > rhsDist;  // Back to front drawing(preserve blending)
              });

    // Prepare indirect draw buffer for both opaque and transparent objects.

    s_RendererData->DrawBufferOpaque->Resize(sizeof(uint32_t) +
                                             s_RendererData->OpaqueObjects.size() * sizeof(DrawMeshTasksIndirectCommand));
    s_RendererData->MeshesDataOpaque->Resize(s_RendererData->OpaqueObjects.size() * sizeof(MeshData));

    std::vector<MeshData> opaqueMeshesData = {};
    for (const auto& [submesh, translation, scale, orientation] : s_RendererData->OpaqueObjects)
    {
        auto& meshData = opaqueMeshesData.emplace_back(
            translation, scale, orientation, submesh->GetBoundingSphere(), submesh->GetVertexPositionBuffer()->GetBindlessIndex(),
            submesh->GetVertexAttributeBuffer()->GetBindlessIndex(), submesh->GetIndexBuffer()->GetBindlessIndex(),
            submesh->GetMeshletVerticesBuffer()->GetBindlessIndex(), submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex(),
            submesh->GetMeshletBuffer()->GetBindlessIndex(), submesh->GetMaterial()->GetBufferIndex());
    }
    if (!opaqueMeshesData.empty())
        s_RendererData->MeshesDataOpaque->SetData(opaqueMeshesData.data(), opaqueMeshesData.size() * sizeof(opaqueMeshesData[0]));

    s_RendererData->CulledMeshesBufferOpaque->Resize(s_RendererData->OpaqueObjects.size() * sizeof(uint32_t));

    auto& transparentObjectCullingPipeline = PipelineLibrary::Get(s_RendererData->TransparentObjectCullingPipelineHash);
    s_RendererData->DrawBufferTransparent->Resize(sizeof(uint32_t) +
                                                  s_RendererData->TransparentObjects.size() * sizeof(DrawMeshTasksIndirectCommand));

    s_RendererData->MeshesDataTransparent->Resize(s_RendererData->TransparentObjects.size() * sizeof(MeshData));
    std::vector<MeshData> transparentMeshesData = {};
    for (const auto& [submesh, translation, scale, orientation] : s_RendererData->TransparentObjects)
    {
        auto& meshData = transparentMeshesData.emplace_back(
            translation, scale, orientation, submesh->GetBoundingSphere(), submesh->GetVertexPositionBuffer()->GetBindlessIndex(),
            submesh->GetVertexAttributeBuffer()->GetBindlessIndex(), submesh->GetIndexBuffer()->GetBindlessIndex(),
            submesh->GetMeshletVerticesBuffer()->GetBindlessIndex(), submesh->GetMeshletTrianglesBuffer()->GetBindlessIndex(),
            submesh->GetMeshletBuffer()->GetBindlessIndex(), submesh->GetMaterial()->GetBufferIndex());
    }
    if (!transparentMeshesData.empty())
        s_RendererData->MeshesDataTransparent->SetData(transparentMeshesData.data(),
                                                       transparentMeshesData.size() * sizeof(transparentMeshesData[0]));

    s_RendererData->CulledMeshesBufferTransparent->Resize(s_RendererData->TransparentObjects.size() * sizeof(uint32_t));

    // Clear buffers for indirect fulfilling.
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferOpaque, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->DrawBufferOpaque, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferOpaque, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferOpaque, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->CulledMeshesBufferOpaque, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferOpaque, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferTransparent, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->DrawBufferTransparent, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferTransparent, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferTransparent, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->FillBuffer(s_RendererData->CulledMeshesBufferTransparent, 0);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferTransparent, EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT,
        EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_TRANSFER_WRITE_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("ObjectCullingPass", glm::vec3(0.1f, 0.1f, 0.1f));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    // 1. Opaque
    PushConstantBlock pc = {};
    pc.CameraDataBuffer  = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.data0.x           = s_RendererData->OpaqueObjects.size();
    pc.addr0             = s_RendererData->DrawBufferOpaque->GetBDA();
    pc.addr1             = s_RendererData->CulledMeshesBufferOpaque->GetBDA();

    auto& opaqueObjectCullingPipeline = PipelineLibrary::Get(s_RendererData->OpaqueObjectCullingPipelineHash);
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], opaqueObjectCullingPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(opaqueObjectCullingPipeline, 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        DivideToNextMultiple(s_RendererData->OpaqueObjects.size(), MESHLET_LOCAL_GROUP_SIZE), 1);

    // 2. Transparents
    pc.data0.x = s_RendererData->TransparentObjects.size();
    pc.addr0   = s_RendererData->DrawBufferTransparent->GetBDA();
    pc.addr1   = s_RendererData->CulledMeshesBufferTransparent->GetBDA();
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], transparentObjectCullingPipeline);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(transparentObjectCullingPipeline, 0, 0, sizeof(pc),
                                                                                       &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
        DivideToNextMultiple(s_RendererData->TransparentObjects.size(), MESHLET_LOCAL_GROUP_SIZE), 1);

    // Next operations will wait until culled objects data filled.
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferOpaque, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT | EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
            EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT,
        EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferOpaque, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT | EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT, EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->DrawBufferTransparent, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT | EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
            EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT,
        EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT | EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
        s_RendererData->CulledMeshesBufferTransparent, EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT | EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT,
        EAccessFlags::ACCESS_SHADER_STORAGE_WRITE_BIT, EAccessFlags::ACCESS_SHADER_STORAGE_READ_BIT);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
}

void Renderer::SSAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->AOFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    PushConstantBlock pc  = {};
    pc.StorageImageIndex  = s_RendererData->AONoiseTexture->GetBindlessIndex();
    pc.AlbedoTextureIndex = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
#if TODO
    pc.MeshIndexBufferIndex = s_RendererData->GBuffer->GetAttachments()[2].m_Index;
#endif

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], PipelineLibrary::Get(s_RendererData->SSAOPipelineHash));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->SSAOPipelineHash), 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->AOFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::HBAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();
    s_RendererData->AOFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    PushConstantBlock pc  = {};
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    pc.AlbedoTextureIndex = s_RendererData->DepthPrePassFramebuffer->GetAttachments()[0].m_Index;
    pc.StorageImageIndex  = s_RendererData->AONoiseTexture->GetBindlessIndex();

    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex], PipelineLibrary::Get(s_RendererData->HBAOPipelineHash));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->HBAOPipelineHash), 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->AOFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BlurAOPass()
{
    if (IsWorldEmpty()) return;

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    PushConstantBlock pc  = {};
    pc.AlbedoTextureIndex = s_RendererData->AOFramebuffer->GetAttachments()[0].m_Index;

    switch (s_RendererSettings.BlurType)
    {
        case EBlurType::BLUR_TYPE_GAUSSIAN:
        {
            for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
            {
                s_RendererData->BlurAOFramebuffer[i]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

                pc.AlbedoTextureIndex = (i == 0 ? s_RendererData->AOFramebuffer->GetAttachments()[0].m_Index
                                                : s_RendererData->BlurAOFramebuffer.at(i - 1)->GetAttachments()[0].m_Index);

                BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                             PipelineLibrary::Get(s_RendererData->BlurAOPipelineHash[i]));
                s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
                    PipelineLibrary::Get(s_RendererData->BlurAOPipelineHash[i]), 0, 0, sizeof(pc), &pc);

                s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);
                s_RendererData->BlurAOFramebuffer[i]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
            }

            break;
        }

        case EBlurType::BLUR_TYPE_MEDIAN:
        {
            s_RendererData->BlurAOFramebuffer[1]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                         PipelineLibrary::Get(s_RendererData->MedianBlurAOPipelineHash));
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
                PipelineLibrary::Get(s_RendererData->MedianBlurAOPipelineHash), 0, 0, sizeof(pc), &pc);

            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);
            s_RendererData->BlurAOFramebuffer[1]->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            break;
        }

        case EBlurType::BLUR_TYPE_BOX:
        {
            s_RendererData->BlurAOFramebuffer[1]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

            BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                         PipelineLibrary::Get(s_RendererData->BoxBlurAOPipelineHash));
            s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
                PipelineLibrary::Get(s_RendererData->BoxBlurAOPipelineHash), 0, 0, sizeof(pc), &pc);

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

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                     PipelineLibrary::Get(s_RendererData->ComputeFrustumsPipelineHash));
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
            PipelineLibrary::Get(s_RendererData->ComputeFrustumsPipelineHash), 0, 0, sizeof(pc), &pc);

        // Divide twice to make use of threads inside warps instead of creating frustum per warp.
        const auto& framebufferSpec = s_RendererData->DepthPrePassFramebuffer->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(framebufferSpec.Width, LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE),
            DivideToNextMultiple(framebufferSpec.Height, LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE));

        // Insert buffer memory barrier to prevent light culling pass reading until frustum computing is done.
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
            s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE_BIT, EAccessFlags::ACCESS_SHADER_READ_BIT);

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
        pc.data0.x                            = s_RendererData->FrustumDebugImage->GetBindlessIndex();
        pc.CameraDataBuffer                   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightDataBuffer                    = s_RendererData->LightsSSBO[s_RendererData->FrameIndex]->GetBDA();
        pc.LightCullingFrustumDataBuffer      = s_RendererData->FrustumsSSBO->GetBDA();
        pc.VisiblePointLightIndicesDataBuffer = s_RendererData->PointLightIndicesSSBO->GetBDA();
        pc.VisibleSpotLightIndicesDataBuffer  = s_RendererData->SpotLightIndicesSSBO->GetBDA();

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ_BIT);

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                     PipelineLibrary::Get(s_RendererData->LightCullingPipelineHash));
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
            PipelineLibrary::Get(s_RendererData->LightCullingPipelineHash), 0, 0, sizeof(pc), &pc);

        const auto& framebufferSpec = s_RendererData->DepthPrePassFramebuffer->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(framebufferSpec.Width, LIGHT_CULLING_TILE_SIZE),
            DivideToNextMultiple(framebufferSpec.Height, LIGHT_CULLING_TILE_SIZE));

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertBufferMemoryBarrier(
            s_RendererData->LightsSSBO[s_RendererData->FrameIndex], EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE_BIT,
            EAccessFlags::ACCESS_SHADER_READ_BIT);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE_BIT,
            EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT, EAccessFlags::ACCESS_SHADER_READ_BIT);
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
        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                     PipelineLibrary::Get(s_RendererData->AtmospherePipelineHash));
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
            PipelineLibrary::Get(s_RendererData->AtmospherePipelineHash), 0, 0, sizeof(pc), &pc);

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
    pc.StorageImageIndex                  = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].m_Index;
    pc.addr0                              = s_RendererData->CulledMeshesBufferOpaque->GetBDA();
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                 PipelineLibrary::Get(s_RendererData->ForwardPlusOpaquePipelineHash));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->ForwardPlusOpaquePipelineHash), 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferOpaque, sizeof(uint32_t), s_RendererData->DrawBufferOpaque, 0,
        (s_RendererData->DrawBufferOpaque->GetSpecification().Capacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand),
        sizeof(DrawMeshTasksIndirectCommand));

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginDebugLabel("TRANSPARENT", glm::vec3(0.5f, 0.9f, 0.2f));
    pc.addr0 = s_RendererData->CulledMeshesBufferTransparent->GetBDA();
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                 PipelineLibrary::Get(s_RendererData->ForwardPlusTransparentPipelineHash));
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->ForwardPlusTransparentPipelineHash), 0, 0, sizeof(pc), &pc);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->DrawMeshTasksMultiIndirect(
        s_RendererData->DrawBufferTransparent, sizeof(uint32_t), s_RendererData->DrawBufferTransparent, 0,
        (s_RendererData->DrawBufferTransparent->GetSpecification().Capacity - sizeof(uint32_t)) / sizeof(DrawMeshTasksIndirectCommand),
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

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                     PipelineLibrary::Get(s_RendererData->SSShadowsPipelineHash));
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
            PipelineLibrary::Get(s_RendererData->SSShadowsPipelineHash), 0, 0, sizeof(pc), &pc);
        const auto& sssImageSpec = s_RendererData->SSShadowsImage->GetSpecification();
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Dispatch(
            DivideToNextMultiple(sssImageSpec.Width, SSS_LOCAL_GROUP_SIZE), DivideToNextMultiple(sssImageSpec.Height, SSS_LOCAL_GROUP_SIZE),
            1);

        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->InsertExecutionBarrier(
            EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT, EAccessFlags::ACCESS_SHADER_WRITE_BIT,
            EPipelineStage::PIPELINE_STAGE_ALL_GRAPHICS_BIT, EAccessFlags::ACCESS_SHADER_READ_BIT);
    }

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndDebugLabel();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
}

void Renderer::BloomPass()
{
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    for (uint32_t i{}; i < s_RendererData->BloomFramebuffer.size(); ++i)
    {
        s_RendererData->BloomFramebuffer[i]->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);

        PushConstantBlock pc  = {};
        pc.AlbedoTextureIndex = (i == 0 ? s_RendererData->GBuffer->GetAttachments().at(1).m_Index
                                        : s_RendererData->BloomFramebuffer.at(i - 1)->GetAttachments().at(0).m_Index);
        pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();

        BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                     PipelineLibrary::Get(s_RendererData->BloomPipelineHash[i]));
        s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
            PipelineLibrary::Get(s_RendererData->BloomPipelineHash[i]), 0, 0, sizeof(pc), &pc);
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
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BeginTimestampQuery();

    s_RendererData->CompositeFramebuffer->BeginPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    BindPipeline(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex],
                 PipelineLibrary::Get(s_RendererData->CompositePipelineHash));

    PushConstantBlock pc  = {};
    pc.StorageImageIndex  = s_RendererData->GBuffer->GetAttachments()[0].m_Index;
    pc.AlbedoTextureIndex = s_RendererData->BloomFramebuffer[1]->GetAttachments()[0].m_Index;
    pc.CameraDataBuffer   = s_RendererData->CameraSSBO[s_RendererData->FrameIndex]->GetBDA();
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->BindPushConstants(
        PipelineLibrary::Get(s_RendererData->CompositePipelineHash), 0, 0, sizeof(pc), &pc);

    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->Draw(3);

    s_RendererData->CompositeFramebuffer->EndPass(s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]);
    s_RendererData->RenderCommandBuffer[s_RendererData->FrameIndex]->EndTimestampQuery();
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
    if (s_RendererData->LightStruct->DirectionalLightCount + 1 > MAX_DIR_LIGHTS)
    {
        LOG_WARN("Max directional light limit reached!");
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
        LOG_WARN("Max point light limit reached!");
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
        LOG_WARN("Max spot light limit reached!");
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
    renderTargetList["AO"]        = s_RendererData->AOFramebuffer->GetAttachments()[0].Attachment;
    renderTargetList["BlurAO"]    = s_RendererData->BlurAOFramebuffer[1]->GetAttachments()[0].Attachment;
    renderTargetList["Heatmap"]   = s_RendererData->LightHeatMapImage;
    renderTargetList["Frustums"]  = s_RendererData->FrustumDebugImage;
    renderTargetList["RayTrace"]  = s_RendererData->RTImage;
    renderTargetList["SSShadows"] = s_RendererData->SSShadowsImage;

    return renderTargetList;
}

}  // namespace Pathfinder