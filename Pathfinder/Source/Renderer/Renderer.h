#pragma once

#include <Core/Core.h>
#include "BindlessRenderer.h"
#include "RendererCoreDefines.h"

#include <Renderer/RenderGraph/RenderGraph.h>

namespace Pathfinder
{

class Texture2D;
class Pipeline;
class Camera;
class Framebuffer;
class Image;
class Submesh;
class Mesh;
class UILayer;

// NOTE: It's not final cuz in future SceneRenderer may derive from this class
// TODO: Remove BindlessRenderer, replace with DescriptorManager -> VulkanDescriptors
// TODO: Introduce QueryManager -> VulkanQueryManager for runtime queries, instead of creating for each command buffer(sucks)

class Renderer : private Uncopyable, private Unmovable
{
  public:
    Renderer();
    virtual ~Renderer();

    static void Init();
    static void Shutdown();

    static void Begin();
    static void Flush(const Unique<UILayer>& uiLayer = nullptr);

    static void BeginScene(const Camera& camera);
    static void EndScene();

    static void SubmitMesh(const Shared<Mesh>& mesh, const glm::vec3& translation = glm::vec3(0.0f),
                           const glm::vec3& scale = glm::vec3(1.0f), const glm::vec4& orientation = glm::vec4(1.f, 0.f, 0.f, 0.f));
    static void AddDirectionalLight(const DirectionalLight& dl);
    static void AddPointLight(const PointLight& pl);
    static void AddSpotLight(const SpotLight& sl);

    static void BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline> pipeline);

    static const std::map<std::string, Shared<Image>> GetRenderTargetList();

    static const std::map<std::string, float>& GetPassStatistics()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData->PassStats;
    }

    static const std::map<std::string, uint64_t>& GetPipelineStatistics()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData->PipelineStats;
    }

    static Shared<Image> GetFinalPassImage();

    NODISCARD FORCEINLINE static const auto& GetRendererData()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData;
    }

    NODISCARD FORCEINLINE static const auto& GetBindlessRenderer()
    {
        PFR_ASSERT(s_BindlessRenderer, "BindlessRenderer is not valid!");
        return s_BindlessRenderer;
    }

    NODISCARD FORCEINLINE static auto& GetRendererSettings() { return s_RendererSettings; }
    NODISCARD FORCEINLINE static auto& GetStats() { return s_RendererStats; }

  private:
    struct RenderObject
    {
        Shared<Submesh> submesh = nullptr;
        glm::vec3 Translation   = glm::vec3(0.f);
        glm::vec3 Scale         = glm::vec3(1.f);
        glm::vec4 Orientation   = glm::vec4(1.f, 0.f, 0.f, 0.f);
    };

    struct RendererData
    {
        // MISC
        Unique<LightData> LightStruct = nullptr;
        CameraData CameraStruct;
        std::map<std::string, float> PassStats;
        std::map<std::string, uint64_t> PipelineStats;

        // NOTE: PerFramed should be objects that are used by host and device.
        BufferPerFrame LightsSSBO;
        BufferPerFrame CameraSSBO;
        BufferPerFrame UploadHeap;

        static constexpr size_t s_MAX_UPLOAD_HEAP_CAPACITY = 4 * 1024 * 1024;  // 4 MB
        uint8_t FrameIndex                                 = 0;

        Weak<Pipeline> LastBoundPipeline;

        // Rendering
        CommandBufferPerFrame RenderCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;
        CommandBufferPerFrame TransferCommandBuffer;

        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        Weak<CommandBuffer> CurrentTransferCommandBuffer;

        Unique<Pathfinder::RenderGraph> RenderGraph = nullptr;

        Shared<Framebuffer> CompositeFramebuffer = nullptr;
        uint64_t CompositePipelineHash           = 0;

        Shared<Image> RTImage          = nullptr;
        uint64_t RTPipelineHash        = 0;
        uint64_t RTComputePipelineHash = 0;
        ShaderBindingTable RTSBT       = {};

        // Forward+ renderer
        Shared<Framebuffer> GBuffer                 = nullptr;
        uint64_t ForwardPlusOpaquePipelineHash      = 0;
        uint64_t ForwardPlusTransparentPipelineHash = 0;

        Shared<Framebuffer> DepthPrePassFramebuffer = nullptr;
        uint64_t DepthPrePassPipelineHash           = 0;

        // Atmospheric Scattering // TODO: Optimize via depth rejection.
        uint64_t AtmospherePipelineHash = 0;

        // BLOOM Ping-pong
        std::array<uint64_t, 2> BloomPipelineHash;
        std::array<Shared<Framebuffer>, 2> BloomFramebuffer;

        /*             SCREEN-SPACE SHADOWS                */
        bool bAnybodyCastsShadows      = false;
        Shared<Image> SSShadowsImage   = nullptr;
        uint64_t SSShadowsPipelineHash = 0;
        /*             SCREEN-SPACE SHADOWS                */

        std::vector<RenderObject> OpaqueObjects;
        std::vector<RenderObject> TransparentObjects;
        Shared<Texture2D> WhiteTexture = nullptr;

        // Light-Culling
        bool bNeedsFrustumsRecomputing       = true;
        uint64_t ComputeFrustumsPipelineHash = 0;
        Shared<Buffer> FrustumsSSBO          = nullptr;
        Shared<Image> FrustumDebugImage      = nullptr;
        Shared<Image> LightHeatMapImage      = nullptr;
        uint64_t LightCullingPipelineHash    = 0;
        Shared<Buffer> PointLightIndicesSSBO = nullptr;
        // NOTE: Instead of creating this shit manually, shader can create you this
        // in e.g. you got writeonly buffer -> shader can create it,
        // in e.g. you got readonly  buffer -> shader gonna wait for you to give it him
        // unordored_map<string,BufferPerFrame>, string maps to set and binding
        Shared<Buffer> SpotLightIndicesSSBO = nullptr;

        // AO
        // TODO: Add HBAO, GTAO, RTAO
        Shared<Texture2D> AONoiseTexture = nullptr;

        Shared<Pathfinder::Framebuffer> AOFramebuffer = nullptr;
        uint64_t SSAOPipelineHash                     = 0;
        uint64_t HBAOPipelineHash                     = 0;

        uint64_t MedianBlurAOPipelineHash = 0;
        uint64_t BoxBlurAOPipelineHash    = 0;
        std::array<uint64_t, 2> BlurAOPipelineHash;  // gaussian
        std::array<Shared<Framebuffer>, 2> BlurAOFramebuffer;

        // Indirect Rendering
        struct ObjectCullStatistics
        {
            uint32_t DrawCountOpaque;
            uint32_t DrawCountTransparent;
        } ObjectCullStats;
        uint64_t OpaqueObjectCullingPipelineHash = 0;
        Shared<Buffer> DrawBufferOpaque          = nullptr;
        Shared<Buffer> MeshesDataOpaque          = nullptr;
        Shared<Buffer> CulledMeshesBufferOpaque  = nullptr;

        uint64_t TransparentObjectCullingPipelineHash = 0;
        Shared<Buffer> DrawBufferTransparent          = nullptr;
        Shared<Buffer> MeshesDataTransparent          = nullptr;
        Shared<Buffer> CulledMeshesBufferTransparent  = nullptr;
    };

    static inline Unique<RendererData> s_RendererData         = nullptr;
    static inline Shared<BindlessRenderer> s_BindlessRenderer = nullptr;

    struct RendererSettings
    {
        bool bVSync;
        bool bDrawColliders;

        EBlurType BlurType;
        EAmbientOcclusionType AOType;
    };

    static inline RendererSettings s_RendererSettings;

    struct RendererStats
    {
        uint32_t TriangleCount;
        uint32_t DescriptorSetCount;
        uint32_t DescriptorPoolCount;
        uint32_t ObjectsDrawn;
        uint32_t BarrierCount;
        float GPUTime;
        float SwapchainPresentTime;
        float RHITime;
        uint32_t ImageViewCount;
        std::vector<MemoryBudget> MemoryBudgets;
    };

    static inline RendererStats s_RendererStats = {};

    static bool IsWorldEmpty()
    {
        return s_RendererData && s_RendererData->TransparentObjects.empty() && s_RendererData->OpaqueObjects.empty();
    }

    static void ObjectCullingPass();
    static void DepthPrePass();
    static void ScreenSpaceShadowsPass();

    static void ComputeFrustumsPass();
    static void LightCullingPass();
    static void AtmosphericScatteringPass();

    static void SSAOPass();
    static void HBAOPass();
    static void BlurAOPass();

    static void GeometryPass();
    static void BloomPass();

    static void RayTracingPass();
    static void CompositePass();
};

}  // namespace Pathfinder