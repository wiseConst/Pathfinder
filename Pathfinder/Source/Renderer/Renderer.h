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

    static void SubmitMesh(const Shared<Mesh>& mesh);

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
    NODISCARD FORCEINLINE static const auto& GetStats() { return s_RendererStats; }

    FORCEINLINE static void SetDescriptorSetCount(const uint32_t descriptorSetCount)
    {
        s_RendererStats.DescriptorSetCount = descriptorSetCount;
    }

    FORCEINLINE static void SetDescriptorPoolCount(const uint32_t descriptorPoolCount)
    {
        s_RendererStats.DescriptorPoolCount = descriptorPoolCount;
    }

  private:
    struct RendererData
    {
        CommandBufferPerFrame RenderCommandBuffer;
        Weak<CommandBuffer> CurrentRenderCommandBuffer;

        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;

        Shared<Pipeline> PathtracingPipeline = nullptr;
        ImagePerFrame PathtracedImage;
        FramebufferPerFrame CompositeFramebuffer;

        // NOTE: Forward+ renderer
        FramebufferPerFrame GBuffer;
        Shared<Pipeline> ForwardRenderingPipeline = nullptr;

        FramebufferPerFrame DepthPrePassFramebuffer;
        Shared<Pipeline> DepthPrePassPipeline = nullptr;

        std::vector<Shared<Submesh>> OpaqueObjects;
        std::vector<Shared<Submesh>> TransparentObjects;
        Shared<Texture2D> WhiteTexture = nullptr;

        Shared<Pipeline> GridPipeline = nullptr;

        // MISC
        uint32_t FrameIndex = 0;
        Pathfinder::CameraData CameraData;
        BufferPerFrame CameraUB;
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
    static void GeometryPass();
};

}  // namespace Pathfinder

#endif  // RENDERER_H
