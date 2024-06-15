#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>

namespace Pathfinder
{
class RenderGraph;

class ObjectCullingPass final
{
  public:
    ObjectCullingPass() = default;

    void AddPass(Unique<RenderGraph>& rendergraph);
};
}  // namespace Pathfinder