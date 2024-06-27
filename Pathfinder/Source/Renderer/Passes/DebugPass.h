#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{
class RenderGraph;

class DebugPass final
{
  public:
    DebugPass() = default;
    DebugPass(const uint32_t width, const uint32_t height, const uint64_t spherePipelineHash, const uint64_t linePipelineHash)
        : m_Width(width), m_Height(height), m_SpherePipelineHash(spherePipelineHash), m_LinePipelineHash(linePipelineHash)
    {
    }

    void AddPass(Unique<RenderGraph>& rendergraph, const LineVertex* lines, const uint32_t lineVertexCount,
                 const std::vector<DebugSphereData>& debugSpheres);
    FORCEINLINE void OnResize(const uint32_t width, const uint32_t height) { m_Width = width, m_Height = height; }

  private:
    uint64_t m_SpherePipelineHash{};
    uint64_t m_LinePipelineHash{};
    uint32_t m_Width{}, m_Height{};

    SurfaceMesh m_DebugSphereMesh = {};
};
}  // namespace Pathfinder