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

// NOTE: It's not final cuz in future SceneRenderer may derive from this class

class Renderer : private Uncopyable, private Unmovable
{
  public:
    Renderer();
    virtual ~Renderer();

    static void Init();
    static void Shutdown();

    static void Begin();
    static void Flush();

    static void BeginScene(const Camera& camera);
    static void EndScene();

    static void SubmitMesh(const Shared<Mesh>& mesh, const glm::mat4& transform = glm::mat4(1.0f));
    static void AddDirectionalLight(const DirectionalLight& dl);
    static void AddPointLight(const PointLight& pl);
    static void AddSpotLight(const SpotLight& sl);

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
    struct RendererData
    {
        // MISC
        Unique<LightsData> LightStruct = nullptr;
        CameraData CameraStruct;
        Frustum CullFrustum;

        // NOTE: PerFramed should be objects that are used by host and device.
        BufferPerFrame LightsSSBO;
        BufferPerFrame CameraUB;
        BufferPerFrame UploadHeap;

        static constexpr size_t s_MAX_UPLOAD_HEAP_CAPACITY = 4 * 1024 * 1024;  // 4 MB
        uint32_t FrameIndex                                = 0;

        Weak<Pipeline> LastBoundPipeline;

        // Rendering
        CommandBufferPerFrame RenderCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;
        CommandBufferPerFrame TransferCommandBuffer;

        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        Weak<CommandBuffer> CurrentTransferCommandBuffer;

        Unique<Pathfinder::RenderGraph> RenderGraph = nullptr;

        Shared<Image> PathtracedImage        = nullptr;
        Shared<Pipeline> PathtracingPipeline = nullptr;

        Shared<Framebuffer> CompositeFramebuffer = nullptr;
        Shared<Pipeline> CompositePipeline       = nullptr;

        // Forward+ renderer
        Shared<Framebuffer> GBuffer          = nullptr;
        Shared<Pipeline> ForwardPlusPipeline = nullptr;

        Shared<Framebuffer> DepthPrePassFramebuffer = nullptr;
        Shared<Pipeline> DepthPrePassPipeline       = nullptr;

        // Atmospheric Scattering
        Shared<Pipeline> AtmospherePipeline = nullptr;

        // TODO: CASCADED SHADOW MAPS
        // DirShadowMaps
        std::vector<Shared<Framebuffer>> DirShadowMaps;
        Shared<Pipeline> DirShadowMapPipeline = nullptr;

        std::vector<RenderObject> OpaqueObjects;
        std::vector<RenderObject> TransparentObjects;
        Shared<Texture2D> WhiteTexture = nullptr;

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
        AO SSAO   = {};
        AO HBAO   = {};
        AO BlurAO = {};
    };
    static inline Unique<RendererData> s_RendererData         = nullptr;
    static inline Shared<BindlessRenderer> s_BindlessRenderer = nullptr;

    struct RendererSettings
    {
        bool bMeshShadingSupport;
        bool bRTXSupport;
        bool bBDASupport;
    };
    static inline RendererSettings s_RendererSettings = {};

    struct RendererStats
    {
        uint32_t TriangleCount;
        uint32_t DescriptorSetCount;
        uint32_t DescriptorPoolCount;
        uint32_t MeshletCount;
        float GPUTime;
        float SwapchainPresentTime;
        float RHITime;
    };
    static inline RendererStats s_RendererStats = {};

    static void BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline>& pipeline);

    // TODO: GPU-side frustum culling
    static bool IsInsideFrustum(const auto& renderObject);

    static void DepthPrePass();
    static void DirShadowMapPass();

    static void LightCullingPass();
    static void AtmosphericScatteringPass();

    static void SSAOPass();
    static void HBAOPass();
    static void BlurAOPass();

    static void GeometryPass();
    static void CompositePass();
};

}  // namespace Pathfinder

#endif  // RENDERER_H
