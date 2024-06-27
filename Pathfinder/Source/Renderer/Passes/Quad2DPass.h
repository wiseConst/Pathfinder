#pragma once

#include <Core/Core.h>
#include <Renderer/RenderGraph/RenderGraphResourceID.h>
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{
class RenderGraph;

class Quad2DPass final
{
  public:
    Quad2DPass() = default;
    Quad2DPass(const uint32_t width, const uint32_t height, const uint64_t pipelineHash);

    void AddPass(Unique<RenderGraph>& rendergraph, std::vector<Sprite>& sprites, const uint32_t spriteCount);
    FORCEINLINE void OnResize(const uint32_t width, const uint32_t height) { m_Width = width, m_Height = height; }

  private:
    uint64_t m_PipelineHash{};
    uint32_t m_Width{}, m_Height{};
};
}  // namespace Pathfinder