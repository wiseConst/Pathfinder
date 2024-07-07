#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>

namespace Pathfinder
{
class RenderGraph;

class FramePreparePass final
{
  public:
    FramePreparePass() = default;

    void AddPass(Unique<RenderGraph>& rendergraph);

  private:
    glm::mat4 CalculateLightSpaceViewProjMatrix(const glm::vec3& lightDir, const float zNear, const float zFar);
    std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& projView);

    void PrepareCSMData();
};
}  // namespace Pathfinder