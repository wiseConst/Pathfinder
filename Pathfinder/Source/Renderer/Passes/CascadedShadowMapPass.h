#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>

namespace Pathfinder
{
class RenderGraph;

class CascadedShadowMapPass final
{
  public:
    CascadedShadowMapPass() = default;
    CascadedShadowMapPass(const uint32_t width, const uint32_t height);

    void AddPass(Unique<RenderGraph>& rendergraph);
    FORCEINLINE void OnResize(const uint32_t width, const uint32_t height) { m_Width = width, m_Height = height; }

  private:
    uint32_t m_Width{}, m_Height{};
};
}  // namespace Pathfinder