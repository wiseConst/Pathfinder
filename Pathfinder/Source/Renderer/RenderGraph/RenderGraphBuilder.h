#pragma once

#include <Core/Core.h>
#include <Renderer/RendererCoreDefines.h>

#include "RenderGraphPass.h"
#include "RenderGraphContext.h"
#include "RenderGraphResourcePool.h"

namespace Pathfinder
{

class RenderGraphBuilder final : private Uncopyable, private Unmovable
{
  public:
    ~RenderGraphBuilder() = default;

    FORCEINLINE void SetViewportScissor(const uint32_t width, const uint32_t height, const uint32_t offsetX = 0, const uint32_t offsetY = 0)
    {
        PFR_ASSERT(m_RGPassBaseRef.m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS,
                   "Only graphics passes can setup viewport and scissor info!");
        m_RGPassBaseRef.m_ViewportScissorInfo = {.Width = width, .Height = height, .OffsetX = offsetX, .OffsetY = offsetY};
    }

    void DeclareBuffer(const std::string& name, const RGBufferSpecification& rgBufferSpec);
    void DeclareTexture(const std::string& name, const RGTextureSpecification& rgTextureSpec);

    RGTextureID WriteTexture(const std::string& name, const std::string& input = s_DEFAULT_STRING);
    RGTextureID ReadTexture(const std::string& name);

    RGTextureID WriteRenderTarget(const std::string& name, const ColorClearValue& clearValue, const EOp loadOp, const EOp storeOp,
                                  const std::string& input = s_DEFAULT_STRING);
    RGTextureID WriteDepthStencil(const std::string& name, const DepthStencilClearValue& clearValue, const EOp depthLoadOp,
                                  const EOp depthStoreOp, const EOp stencilLoadOp = EOp::DONT_CARE,
                                  const EOp stencilStoreOp = EOp::DONT_CARE, const std::string& input = s_DEFAULT_STRING);

    // NOTE: Can be extended in future by adding EResourceState::storage/vertex/index_buffer
    RGBufferID WriteBuffer(const std::string& name, const std::string& input = s_DEFAULT_STRING);
    RGBufferID ReadBuffer(const std::string& name);

  private:
    RenderGraph& m_RenderGraphRef;
    RGPassBase& m_RGPassBaseRef;

    friend class RenderGraph;
    RenderGraphBuilder() = delete;

    RenderGraphBuilder(RenderGraph& renderGraphRef, RGPassBase& rgPassBaseRef)
        : m_RenderGraphRef(renderGraphRef), m_RGPassBaseRef(rgPassBaseRef)
    {
    }
};
using RGBuilder = RenderGraphBuilder;

}  // namespace Pathfinder