#pragma once

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

#include <Renderer/Passes/DebugPass.h>

namespace Pathfinder
{

class Pipeline;
class Mesh;
class RenderGraph;

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
        s_DebugRendererData->DebugSpheres.emplace_back(translation, scale, orientation, center, radius, glm::packUnorm4x8(color));
    }

    static void Flush(Unique<RenderGraph>& renderGraph);

  private:
    struct DebugRendererData
    {
        Pathfinder::DebugPass DebugPass;
        uint64_t LinePipelineHash = 0;

        uint64_t SpherePipelineHash = 0;
        std::vector<DebugSphereData> DebugSpheres;

        using LineVertexBasePerFrame = std::array<LineVertex*, s_FRAMES_IN_FLIGHT>;
        LineVertexBasePerFrame LineVertexBase;
        LineVertexBasePerFrame LineVertexCurrent;

        uint32_t LineVertexCount = 0;
        float LineWidth          = 2.5f;
        uint8_t FrameIndex       = 0;
    };
    static inline Unique<DebugRendererData> s_DebugRendererData = nullptr;

    static void UpdateState();

    DebugRenderer()  = delete;
    ~DebugRenderer() = default;
};

}  // namespace Pathfinder
