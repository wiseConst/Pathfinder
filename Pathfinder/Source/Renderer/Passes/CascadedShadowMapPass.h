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
    CascadedShadowMapPass(const glm::uvec2& dimensions);

    void AddPass(Unique<RenderGraph>& rendergraph);
    void SetShadowMapDimensions(const glm::uvec2& dimensions) noexcept { m_Dimensions = dimensions; }

  private:
    glm::uvec2 m_Dimensions{1024};
};
}  // namespace Pathfinder