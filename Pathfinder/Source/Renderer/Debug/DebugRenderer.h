#pragma once

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
    static void DrawAABB(const glm::vec3& center, const glm::vec3& halfExtents, const glm::mat4& transform, const glm::vec4& color);

    static void DrawRect(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);
    static void DrawRect(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec4& color);

    static void DrawSphere(const Shared<Mesh>& mesh, const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                           const glm::vec4& color);
    FORCEINLINE static void DrawSphere(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                                       const glm::vec3& center, const float radius, const glm::vec4& color)
    {
        s_DebugRendererData->DebugSpheres.emplace_back(translation, scale, orientation, center, radius, PackVec4ToU8Vec4(color));
    }

    static void Flush(Shared<CommandBuffer>& renderCommandBuffer);

  private:
    struct DebugSphereData
    {
        glm::vec3 Translation = glm::vec3(0.f);
        glm::vec3 Scale       = glm::vec3(1.f);
        glm::vec4 Orientation = glm::vec4(0.f, 0.f, 0.f,1.f);
        glm::vec3 Center      = glm::vec3(0.f);
        float Radius          = 0.f;
        glm::u8vec4 Color     = glm::u8vec4(1);
    };

    struct DebugRendererData
    {
        BufferPerFrame LineVertexBuffer;
        uint64_t LinePipelineHash = 0;

        SurfaceMesh DebugSphereMesh = {};
        uint64_t SpherePipelineHash = 0;

        BufferPerFrame DebugSphereSSBO;
        std::vector<DebugSphereData> DebugSpheres;

        using LineVertexBasePerFrame = std::array<LineVertex*, s_FRAMES_IN_FLIGHT>;
        LineVertexBasePerFrame LineVertexBase;
        LineVertexBasePerFrame LineVertexCurrent;

        uint8_t FrameIndex       = 0;
        uint32_t LineVertexCount = 0;
        float LineWidth          = 2.5f;

        static constexpr uint32_t s_MAX_LINES              = 1000;
        static constexpr uint32_t s_MAX_VERTICES           = s_MAX_LINES * 4;
        static constexpr uint32_t s_MAX_VERTEX_BUFFER_SIZE = s_MAX_VERTICES * sizeof(LineVertex);

        static inline std::array<glm::vec4, 4> QuadVertices = {
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
