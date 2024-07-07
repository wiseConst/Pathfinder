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
    FORCEINLINE void OnResize(const uint32_t width, const uint32_t height)
    {
        const auto maxDim = std::max(width, height);

        // NOTE: To prevent jiggering near edges of shadows when rotating camera
        // dimensions of shadow map should be the same.
        m_Width = m_Height = maxDim;
    }

  private:
    uint32_t m_Width{}, m_Height{};
};
}  // namespace Pathfinder