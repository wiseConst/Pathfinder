#ifndef RENDERER_H
#define RENDERER_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Texture2D;
class Pipeline;
class Camera;

// TODO: Make fallback to default graphics pipeline; as of now I imply mesh shading support
// It's not final cuz in future SceneRenderer may derive from this class
class Renderer : private Uncopyable, private Unmovable
{
  public:
    Renderer()          = default;
    virtual ~Renderer() = default;

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

  private:
    struct RendererData
    {
        Weak<CommandBuffer> CurrentRenderCommandBuffer;
        CommandBufferPerFrame RenderCommandBuffer;
        uint32_t FrameIndex = 0;

        Shared<Texture2D> WhiteTexture = nullptr;
    };
    static inline Unique<RendererData> s_RendererData = nullptr;
};

}  // namespace Pathfinder

#endif  // RENDERER_H
