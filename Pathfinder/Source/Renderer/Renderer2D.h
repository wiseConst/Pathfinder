#pragma once

#include <Core/Core.h>
#include "RendererCoreDefines.h"

#include <Renderer/Passes/Quad2DPass.h>

namespace Pathfinder
{

class Pipeline;
class Texture;
class RenderGraph;

class Renderer2D final : private Uncopyable, private Unmovable
{
  public:
    Renderer2D() { Init(); };
    ~Renderer2D() { Shutdown(); };

    void Begin(const uint8_t frameIndex);
    void Flush(Unique<RenderGraph>& renderGraph);

    void DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                  const glm::vec4& color = glm::vec4(1.0f), const Shared<Texture>& texture = nullptr, const uint32_t layer = 0);

    NODISCARD FORCEINLINE auto& GetStats() { return m_Renderer2DStats; }

  private:
    struct RendererData2D
    {
        uint8_t FrameIndex        = 0;
        uint64_t QuadPipelineHash = 0;

        std::vector<Sprite> Sprites;
        uint32_t CurrentSpriteCount = 0;

        Pathfinder::Quad2DPass Quad2DPass;
    };
    Unique<RendererData2D> m_RendererData2D = nullptr;

    struct Renderer2DStats
    {
        uint32_t QuadCount;
        uint32_t TriangleCount;
    };
    Renderer2DStats m_Renderer2DStats = {};

    void Init();
    void Shutdown();
};

}  // namespace Pathfinder
