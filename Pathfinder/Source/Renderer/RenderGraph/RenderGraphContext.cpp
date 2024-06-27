#include <PathfinderPCH.h>
#include "RenderGraphContext.h"
#include "RenderGraph.h"
#include "RenderGraphPass.h"

namespace Pathfinder
{

RenderGraphContext::RenderGraphContext(RenderGraph& rg, RenderGraphPassBase& rgPassBase) : m_RenderGraphRef(rg), m_RGPassBaseRef(rgPassBase)
{
}

NODISCARD Shared<Texture>& RenderGraphContext::GetTexture(const RGTextureID resourceID) const
{
    return m_RenderGraphRef.GetTexture(resourceID);
}

NODISCARD Shared<Buffer>& RenderGraphContext::GetBuffer(const RGBufferID resourceID) const
{
    return m_RenderGraphRef.GetBuffer(resourceID);
}

}  // namespace Pathfinder