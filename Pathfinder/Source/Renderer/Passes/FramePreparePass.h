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
};
}  // namespace Pathfinder