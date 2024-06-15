#include <PathfinderPCH.h>
#include "RenderGraphBuilder.h"
#include "RenderGraph.h"

namespace Pathfinder
{

void RenderGraphBuilder::DeclareBuffer(const std::string& name, const RGBufferSpecification& rgBufferSpec)
{
    m_RGPassBaseRef.m_BufferCreates.insert(m_RenderGraphRef.DeclareBuffer(name, rgBufferSpec));
}

void RenderGraphBuilder::DeclareTexture(const std::string& name, const RGTextureSpecification& rgTextureSpec)
{
    m_RGPassBaseRef.m_TextureCreates.insert(m_RenderGraphRef.DeclareTexture(name, rgTextureSpec));
}

RGTextureID RenderGraphBuilder::WriteTexture(const std::string& name, const std::string& input)
{
    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    if (input != s_DEFAULT_STRING)
    {
    }

    //    m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_STORAGE_IMAGE;
    return textureID;
}

RGTextureID Pathfinder::RenderGraphBuilder::ReadTexture(const std::string& name)
{
    return RGTextureID();
}

RGTextureID RenderGraphBuilder::WriteRenderTarget(const std::string& name, const ColorClearValue& clearValue, const EOp loadOp,
                                                  const EOp storeOp, const std::string& input)
{
    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    if (input != s_DEFAULT_STRING)
    {
        m_RGPassBaseRef.m_TextureReads.insert(textureID);
        //    m_RenderGraphRef.AddRMWAlias(name, input);
    }

    m_RGPassBaseRef.m_RenderTargetsInfo.emplace_back(clearValue, textureID, loadOp, storeOp);
    //   m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_RENDER_TARGET;
    return textureID;
}

RGTextureID RenderGraphBuilder::WriteDepthStencil(const std::string& name, const DepthStencilClearValue& clearValue, const EOp depthLoadOp,
                                                  const EOp depthStoreOp, const EOp stencilLoadOp, const EOp stencilStoreOp,
                                                  const std::string& input)
{
    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    m_RGPassBaseRef.m_DepthStencil = {.ClearValue         = clearValue,
                                      .DepthStencilHandle = textureID,
                                      .DepthLoadOp        = depthLoadOp,
                                      .DepthStoreOp       = depthStoreOp,
                                      .StencilLoadOp      = stencilLoadOp,
                                      .StencilStoreOp     = stencilStoreOp};
    //   m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_DEPTH_WRITE;
    return textureID;
}

RGBufferID RenderGraphBuilder::ReadBuffer(const std::string& name)
{
    const auto bufferID = m_RenderGraphRef.GetBufferID(name);
    m_RGPassBaseRef.m_BufferReads.insert(bufferID);

    //   m_RGPassBaseRef.m_BufferStateMap[bufferID.m_ID.value()] = EResourceState::RESOURCE_STATE_STORAGE_BUFFER;
    return bufferID;
}

RGBufferID Pathfinder::RenderGraphBuilder::WriteBuffer(const std::string& name, const std::string& input)
{
    const auto bufferID = m_RenderGraphRef.GetBufferID(name);
    m_RGPassBaseRef.m_BufferWrites.insert(bufferID);

    if (input != s_DEFAULT_STRING)
    {
        m_RGPassBaseRef.m_BufferReads.insert(bufferID);
        //    m_RenderGraphRef.AddRMWAlias(name, input);
    }

    //  m_RGPassBaseRef.m_BufferStateMap[bufferID.m_ID.value()] = EResourceState::RESOURCE_STATE_STORAGE_BUFFER;
    return bufferID;
}

}  // namespace Pathfinder