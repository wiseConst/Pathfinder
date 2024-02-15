#ifndef RENDERER_H
#define RENDERER_H

#include "Core/Core.h"
#include "BindlessRenderer.h"
#include "RendererCoreDefines.h"

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

// TODO: Implement RT_Renderer / RayTracingModule / RayTracingBuilder
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
    struct RenderObject
    {
        Shared<Submesh> submesh = nullptr;
        glm::mat4 Transform     = glm::mat4(1.0f);
    };
    // TODO: Realign structure
    struct RendererData
    {
        // MISC
        LightsData LightStruct;
        CameraData CameraStruct;

        BufferPerFrame LightsUB;
        BufferPerFrame CameraUB;
        BufferPerFrame UploadHeap;

        static constexpr size_t s_MAX_UPLOAD_HEAP_CAPACITY = 4 * 1024 * 1024;  // 4 MB
        uint32_t FrameIndex                                = 0;

        // Rendering
        CommandBufferPerFrame RenderCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;
        CommandBufferPerFrame TransferCommandBuffer;

        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        Weak<CommandBuffer> CurrentTransferCommandBuffer;

        ImagePerFrame PathtracedImage;
        Shared<Pipeline> PathtracingPipeline = nullptr;
        FramebufferPerFrame CompositeFramebuffer;
        Shared<Pipeline> CompositePipeline = nullptr;

        // NOTE: Forward+ renderer
        FramebufferPerFrame GBuffer;
        Shared<Pipeline> ForwardPlusPipeline = nullptr;

        FramebufferPerFrame DepthPrePassFramebuffer;
        Shared<Pipeline> DepthPrePassPipeline = nullptr;

        std::vector<RenderObject> OpaqueObjects;
        std::vector<RenderObject> TransparentObjects;
        Shared<Texture2D> WhiteTexture = nullptr;

        Shared<Pipeline> GridPipeline = nullptr;

        ImagePerFrame FrustumDebugImage;
        Shared<Pipeline> LightCullingPipeline = nullptr;
        BufferPerFrame PointLightIndicesStorageBuffer;
        // NOTE: Instead of creating this shit manually, shader can create you this
        // in e.g. you got writeonly buffer -> shader can create it,
        // in e.g. you got readonly  buffer -> shader gonna wait for you to give it him
        // unordored_map<string,BufferPerFrame>, string maps to set and binding
        BufferPerFrame SpotLightIndicesStorageBuffer;  // TODO: Implement

        // TODO: Add HBAO, GTAO, RTAO
        // AO
        Shared<Pipeline> SSAOPipeline = nullptr;
        FramebufferPerFrame SSAOFramebuffer;
        Shared<Texture2D> SSAONoiseTexture = nullptr;
        Shared<Pipeline> SSAOBlurPipeline  = nullptr;
        FramebufferPerFrame SSAOBlurFramebuffer;
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
    };
    static inline RendererStats s_RendererStats = {};

    static void DrawGrid();
    static void DepthPrePass();

    static void LightCullingPass();
    static void SSAOPass();

    static void GeometryPass();
};

}  // namespace Pathfinder

#endif  // RENDERER_H
