#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>

namespace Pathfinder
{
class RenderGraph;

class LightCullingPass final
{
  public:
    LightCullingPass() = default;
    LightCullingPass(const uint32_t width, const uint32_t height);

    void AddPass(Unique<RenderGraph>& rendergraph);
    FORCEINLINE void OnResize(const uint32_t width, const uint32_t height)
    {
        m_Width = width, m_Height = height;
        m_bRecomputeLightCullingFrustums = true;
    }

    FORCEINLINE void SetRecomputeLightCullFrustums(const bool bRecompute) { m_bRecomputeLightCullingFrustums = bRecompute; }

  private:
    uint32_t m_Width{}, m_Height{};
    bool m_bRecomputeLightCullingFrustums = true;

    void AddLightCullingPass(Unique<RenderGraph>& rendergraph);
    void AddComputeLightCullingFrustumsPass(Unique<RenderGraph>& rendergraph);
};
}  // namespace Pathfinder