#ifndef RENDERGRAPH_BUILDER_H
#define RENDERGRAPH_BUILDER_H

#include "Core/Core.h"

namespace Pathfinder
{

class RenderGraph;
class RenderGraphBuilder final : private Uncopyable, private Unmovable
{
  public:
    RenderGraphBuilder()           = delete;
    ~RenderGraphBuilder() override = default;

    static Unique<RenderGraph> Create(const std::filesystem::path& renderGraphSpecificationPath);

  private:
};

}  // namespace Pathfinder

#endif