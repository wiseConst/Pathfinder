#ifndef RENDERER2D_H
#define RENDERER2D_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Pipeline;

/*
struct Sprite
{
    Ref<Texture2D> Texture = nullptr;
    glm::mat4 Transform    = glm::mat4(1.0f);
    glm::vec4 Color        = glm::vec4(1.0f);
    glm::vec2 SpriteCoords = glm::vec2(0.0f);
    uint32_t Layer         = 0;
};
*/

class Renderer2D final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    static void Begin();
    static void Flush();

    static void DrawQuad(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

    NODISCARD FORCEINLINE static auto& GetStats() { return s_Renderer2DStats; }

    NODISCARD FORCEINLINE static const auto& GetRendererData()
    {
        PFR_ASSERT(s_RendererData2D, "RendererData is not valid!");
        return s_RendererData2D;
    }

  private:
    struct RendererData2D
    {
        uint32_t FrameIndex = 0;

        using QuadVertexBasePerFrame = std::array<QuadVertex*, s_FRAMES_IN_FLIGHT>;
        QuadVertexBasePerFrame QuadVertexBase;
        QuadVertexBasePerFrame QuadVertexCurrent;

        BufferPerFrame QuadVertexBuffer;
        Shared<Pipeline> QuadPipeline = nullptr;

        static constexpr uint32_t s_MAX_QUADS    = 2500;
        static constexpr uint32_t s_MAX_VERTICES = s_MAX_QUADS * 4;
        static constexpr uint32_t s_MAX_INDICES  = s_MAX_QUADS * 6;

        Shared<Buffer> QuadIndexBuffer = nullptr;

        static constexpr glm::vec3 QuadVertices[4] = {
            glm::vec3(-0.5f, -0.5f, 0.0f),  // bottom left
            glm::vec3(0.5f, -0.5f, 0.0f),   // bottom right
            glm::vec3(0.5f, 0.5f, 0.0f),    // top right
            glm::vec3(-0.5f, 0.5f, 0.0f),   // top left
        };

        static constexpr glm::vec2 QuadUVs[4] = {
            glm::vec2(0.0f, 0.0f),  // bottom left
            glm::vec2(1.0f, 0.0f),  // bottom right
            glm::vec2(1.0f, 1.0f),  // top right
            glm::vec2(0.0f, 1.0f),  // top left
        };

        // FIXME: Currently wrong
        static constexpr glm::vec3 QuadNormals[4] = {
            glm::vec3(1.0f, 0.0f, 0.0f),   //  top right
            glm::vec3(0.0f, -1.0f, 0.0f),  // bottom right
            glm::vec3(-1.0f, 0.0f, 0.0f),  // bottom left
            glm::vec3(0.0f, 1.0f, 0.0f),   // top left
        };
    };
    static inline Unique<RendererData2D> s_RendererData2D = nullptr;

    struct Renderer2DStats
    {
        uint32_t BatchCount    = 0;
        uint32_t QuadCount     = 0;
        uint32_t TriangleCount = 0;
    } static inline s_Renderer2DStats = {};

    Renderer2D();
    ~Renderer2D() override;
};

}  // namespace Pathfinder

#endif  // RENDERER2D_H
