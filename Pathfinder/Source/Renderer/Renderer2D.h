#ifndef RENDERER2D_H
#define RENDERER2D_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Pipeline;

class Renderer2D final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    static void DrawQuad(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

  private:
    struct RendererData2D
    {
        // VertexBufferPerFrame QuadVertexBuffer;
        Shared<Pipeline> QuadPipeline = nullptr;
    };
    static inline Unique<RendererData2D> s_RendererData2D = nullptr;

    Renderer2D()           = default;
    ~Renderer2D() override = default;
};

}  // namespace Pathfinder

#endif  // RENDERER2D_H
