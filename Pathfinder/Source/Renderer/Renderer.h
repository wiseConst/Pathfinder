#ifndef RENDERER_H
#define RENDERER_H

#include "Core/Core.h"
#include "BindlessRenderer.h"
#include "RendererCoreDefines.h"
#include "RenderGraph/RenderGraph.h"

namespace Pathfinder
{

class Texture2D;
class TextureCube;
class Pipeline;
class Camera;
class Framebuffer;
class Image;
class Submesh;
class Mesh;
class UILayer;

// NOTE: It's not final cuz in future SceneRenderer may derive from this class

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

    static void SubmitMesh(const Shared<Mesh>& mesh, const glm::mat4& transform = glm::mat4(1.0f));
    static void AddDirectionalLight(const DirectionalLight& dl);
    static void AddPointLight(const PointLight& pl);
    static void AddSpotLight(const SpotLight& sl);

    static void BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline>& pipeline);

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
        glm::mat4 Transform     = glm::mat4(1.0f);
    };

    struct RendererData
    {
        // MISC
        Unique<LightsData> LightStruct = nullptr;
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
        Shared<Pipeline> CompositePipeline       = nullptr;

        // Forward+ renderer
        Shared<Framebuffer> GBuffer                     = nullptr;
        Shared<Pipeline> ForwardPlusOpaquePipeline      = nullptr;
        Shared<Pipeline> ForwardPlusTransparentPipeline = nullptr;

        Shared<Framebuffer> DepthPrePassFramebuffer = nullptr;
        Shared<Pipeline> DepthPrePassPipeline       = nullptr;

        // Atmospheric Scattering // TODO: Optimize via depth rejection.
        Shared<Pipeline> AtmospherePipeline = nullptr;

        // BLOOM Ping-pong
        std::array<Shared<Pipeline>, 2> BloomPipeline;
        std::array<Shared<Framebuffer>, 2> BloomFramebuffer;

        /*              SHADOWS                */
        EShadowSetting CurrentShadowSetting = EShadowSetting::SHADOW_SETTING_MEDIUM;

        Shared<Image> CascadeDebugImage = nullptr;

        // DirShadowMaps
        std::vector<Shared<Framebuffer>> DirShadowMaps;
        Shared<Pipeline> DirShadowMapPipeline = nullptr;

        // TODO: How do I handle them properly?
        // PointLightShadowMaps
        struct PointLightShadowMapInfo
        {
            Shared<Framebuffer> PointLightShadowMap = nullptr;
            uint32_t LightAndMatrixIndex            = INVALID_LIGHT_INDEX;  // PointLight array index, index into viewproj
        };
        std::vector<PointLightShadowMapInfo> PointLightShadowMapInfos;
        Shared<Pipeline> PointLightShadowMapPipeline = nullptr;
        /*              SHADOWS                */

        std::vector<RenderObject> OpaqueObjects;
        std::vector<RenderObject> TransparentObjects;
        Shared<Texture2D> WhiteTexture = nullptr;

        // Light-Culling
        bool bNeedsFrustumsRecomputing                = true;
        Shared<Pipeline> ComputeFrustumsPipeline      = nullptr;
        Shared<Buffer> FrustumsSSBO                   = nullptr;
        Shared<Image> FrustumDebugImage               = nullptr;
        Shared<Image> LightHeatMapImage               = nullptr;
        Shared<Pipeline> LightCullingPipeline         = nullptr;
        Shared<Buffer> PointLightIndicesStorageBuffer = nullptr;
        // NOTE: Instead of creating this shit manually, shader can create you this
        // in e.g. you got writeonly buffer -> shader can create it,
        // in e.g. you got readonly  buffer -> shader gonna wait for you to give it him
        // unordored_map<string,BufferPerFrame>, string maps to set and binding
        Shared<Buffer> SpotLightIndicesStorageBuffer = nullptr;

        // AO
        // TODO: Add HBAO, GTAO, RTAO
        Shared<Texture2D> AONoiseTexture = nullptr;
        struct AO
        {
            Shared<Pathfinder::Pipeline> Pipeline       = nullptr;
            Shared<Pathfinder::Framebuffer> Framebuffer = nullptr;
        };
        AO SSAO                               = {};
        AO HBAO                               = {};
        Shared<Pipeline> MedianBlurAOPipeline = nullptr;
        std::array<Shared<Pipeline>, 2> BlurAOPipeline;  // gaussian
        std::array<Shared<Framebuffer>, 2> BlurAOFramebuffer;

        // Indirect Rendering
        struct ObjectCullStatistics
        {
            uint32_t DrawCountOpaque;
            uint32_t DrawCountTransparent;
        } ObjectCullStats;

        Shared<Buffer> DrawBufferOpaque         = nullptr;
        Shared<Buffer> MeshesDataOpaque         = nullptr;
        Shared<Buffer> CulledMeshesBufferOpaque = nullptr;

        Shared<Buffer> DrawBufferTransparent         = nullptr;
        Shared<Buffer> MeshesDataTransparent         = nullptr;
        Shared<Buffer> CulledMeshesBufferTransparent = nullptr;

        Shared<Pipeline> ObjectCullingPipeline = nullptr;
    };
    static inline Unique<RendererData> s_RendererData         = nullptr;
    static inline Shared<BindlessRenderer> s_BindlessRenderer = nullptr;

    struct RendererSettings
    {
        bool bVSync;

        EBlurType BlurType = EBlurType::BLUR_TYPE_GAUSSIAN;
    };
    static inline RendererSettings s_RendererSettings = {};

    struct RendererStats
    {
        uint32_t TriangleCount;
        uint32_t DescriptorSetCount;
        uint32_t DescriptorPoolCount;
        uint32_t MeshletCount;
        uint32_t ObjectsDrawn;
        float GPUTime;
        float SwapchainPresentTime;
        float RHITime;
        std::vector<MemoryBudget> MemoryBudgets;
    };
    static inline RendererStats s_RendererStats = {};

    static bool IsWorldEmpty()
    {
        return s_RendererData && s_RendererData->TransparentObjects.empty() && s_RendererData->OpaqueObjects.empty();
    }
    static void ObjectCullingPass();

    static void DepthPrePass();
    static void DirShadowMapPass();
    static void PointLightShadowMapPass();

    static void ComputeFrustumsPass();
    static void LightCullingPass();
    static void AtmosphericScatteringPass();

    static void SSAOPass();
    static void HBAOPass();
    static void BlurAOPass();

    static void GeometryPass();
    static void BloomPass();

    static void CompositePass();
};

}  // namespace Pathfinder

#endif  // RENDERER_H
