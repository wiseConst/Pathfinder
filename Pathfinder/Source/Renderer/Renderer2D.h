#pragma once

#include <Core/Core.h>
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Pipeline;
class Texture2D;

// struct SpriteProxy
//{
//     glm::vec3 Translation     = glm::vec3(0.0f);
//     glm::vec3 Scale           = glm::vec3(1.0f);
//     glm::vec4 Orientation     = glm::vec4(1.0f, 0.f, 0.f, 0.f);
//     glm::vec4 Color           = glm::vec4(1.0f);
//     Shared<Texture2D> Texture = nullptr;
//     uint32_t Layer            = 0;  // Only for sorting.
// };

class Renderer2D final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    static void Begin();
    static void Flush(Shared<CommandBuffer>& renderCommandBuffer);

    static void DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                         const glm::vec4& color = glm::vec4(1.0f), const Shared<Texture2D>& texture = nullptr, const uint32_t layer = 0);

    NODISCARD FORCEINLINE static auto& GetStats() { return s_Renderer2DStats; }

    NODISCARD FORCEINLINE static const auto& GetRendererData()
    {
        PFR_ASSERT(s_RendererData2D, "RendererData is not valid!");
        return s_RendererData2D;
    }

  private:
    struct RendererData2D
    {
        static constexpr uint32_t s_MAX_QUADS = 25000;
        uint8_t FrameIndex                    = 0;
        uint64_t QuadPipelineHash             = 0;

        BufferPerFrame SpriteSSBO;
        std::array<Sprite, s_MAX_QUADS> Sprites;
        uint32_t CurrentSpriteCount = 0;
    };
    static inline Unique<RendererData2D> s_RendererData2D = nullptr;

    struct Renderer2DStats
    {
        uint32_t QuadCount;
        uint32_t TriangleCount;
        float GPUTime;
    };
    static inline Renderer2DStats s_Renderer2DStats = {};

    Renderer2D()  = delete;
    ~Renderer2D() = default;
};

}  // namespace Pathfinder
