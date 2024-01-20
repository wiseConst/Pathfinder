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
class Mesh;
class Framebuffer;
class Image;
class Submesh;

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
        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        CommandBufferPerFrame RenderCommandBuffer;

        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;

        Shared<Pipeline> PathtracingPipeline = nullptr;
        ImagePerFrame PathtracedImage;
        FramebufferPerFrame CompositeFramebuffer;

        FramebufferPerFrame GBuffer;

        // NOTE: Forward+ renderer
        Shared<Pipeline> ForwardRenderingPipeline = nullptr;

        FramebufferPerFrame DepthPrePassFramebuffer;
        Shared<Pipeline> DepthPrePassPipeline = nullptr;

        std::vector<Shared<Submesh>> OpaqueObjects;
        std::vector<Shared<Submesh>> TransparentObjects;
        //  Shared<Texture2D> WhiteTexture = nullptr;

        // MISC
        uint32_t FrameIndex = 0;
        Pathfinder::CameraData CameraData;
        BufferPerFrame CameraUB;
    };
    static inline Unique<RendererData> s_RendererData         = nullptr;
    static inline Shared<BindlessRenderer> s_BindlessRenderer = nullptr;

    struct RendererSettings
    {
        bool bMeshShadingSupport = false;
        bool bRTXSupport         = false;
    } static inline s_RendererSettings = {};

    struct RendererStats
    {
        uint32_t TriangleCount       = 0;
        uint32_t DescriptorSetCount  = 0;
        uint32_t DescriptorPoolCount = 0;
        uint32_t MeshletCount        = 0;
        float GPUTime                = 0.0F;
    } static inline s_RendererStats = {};

    static void GeometryPass();
};

}  // namespace Pathfinder

#endif  // RENDERER_H
