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

// TODO: Make fallback to default graphics pipeline; as of now I imply mesh shading support
// It's not final cuz in future SceneRenderer may derive from this class
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

  private:
    struct RendererData
    {
        uint32_t FrameIndex = 0;

        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        CommandBufferPerFrame RenderCommandBuffer;

        Weak<CommandBuffer> CurrentComputeCommandBuffer;
        CommandBufferPerFrame ComputeCommandBuffer;

        Shared<Pipeline> PathtracingPipeline = nullptr;
        ImagePerFrame PathtracedImage;
        FramebufferPerFrame CompositeFramebuffer;

        FramebufferPerFrame GBuffer;

        std::vector<Shared<Mesh>> OpaqueObjects;
        std::vector<Shared<Mesh>> TranslucentObjects;
        //  Shared<Texture2D> WhiteTexture = nullptr;
    };
    static inline Unique<RendererData> s_RendererData         = nullptr;
    static inline Shared<BindlessRenderer> s_BindlessRenderer = nullptr;
};

}  // namespace Pathfinder

#endif  // RENDERER_H
