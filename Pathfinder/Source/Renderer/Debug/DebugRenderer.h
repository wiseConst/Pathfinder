#ifndef DEBUGRENDERER_H
#define DEBUGRENDERER_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{

class Pipeline;
class Mesh;

class DebugRenderer
{
  public:
    static void Init();
    static void Shutdown();

    static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color);
    static void DrawAABB(const Shared<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color);
    static void DrawRect(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
    static void DrawRect(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
    static void DrawSphere(const Shared<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color);

    static void Flush();

  private:
    struct DebugRendererData
    {
        BufferPerFrame LineVertexBuffer;
        Shared<Pipeline> LinePipeline = nullptr;

        using LineVertexBasePerFrame = std::array<LineVertex*, s_FRAMES_IN_FLIGHT>;
        LineVertexBasePerFrame LineVertexBase;
        LineVertexBasePerFrame LineVertexCurrent;

        uint32_t FrameIndex      = 0;
        uint32_t LineVertexCount = 0;
        float LineWidth          = 2.5f;

        static constexpr uint32_t s_MAX_LINES              = 1000;
        static constexpr uint32_t s_MAX_VERTICES           = s_MAX_LINES * 4;
        static constexpr uint32_t s_MAX_VERTEX_BUFFER_SIZE = s_MAX_VERTICES * sizeof(LineVertex);

        static constexpr std::array<glm::vec4, 4> QuadVertices = {
            glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),  // bottom left
            glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),   // bottom right
            glm::vec4(0.5f, 0.5f, 0.0f, 1.0f),    // top right
            glm::vec4(-0.5f, 0.5f, 0.0f, 1.0f)    // top left
        };
    };
    static inline Unique<DebugRendererData> s_DebugRendererData = nullptr;

    static void UpdateState();

    DebugRenderer()  = delete;
    ~DebugRenderer() = default;
};

}  // namespace Pathfinder

#endif