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
    if (input != s_DEFAULT_STRING)
    {
        const auto sourceTextureID = m_RenderGraphRef.GetTextureID(input);
        m_RGPassBaseRef.m_TextureReads.insert(sourceTextureID);

        const auto textureID = m_RenderGraphRef.AliasTexture(name, input);
        m_RGPassBaseRef.m_TextureWrites.insert(textureID);

        m_RenderGraphRef.GetRGTexture(textureID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
        m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);

        m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_STORAGE_IMAGE;
        return textureID;
    }

    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_STORAGE_IMAGE;
    return textureID;
}

RGTextureID RenderGraphBuilder::ReadTexture(const std::string& name, const ResourceStateFlags resourceStateFlags)
{
    PFR_ASSERT(resourceStateFlags != 0, "Resource state flags can't be empty!");

    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureReads.insert(textureID);

    m_RenderGraphRef.GetRGTexture(textureID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] |= EResourceState::RESOURCE_STATE_TEXTURE | resourceStateFlags;
    return textureID;
}

RGTextureID RenderGraphBuilder::WriteRenderTarget(const std::string& name, const ColorClearValue& clearValue, const EOp loadOp,
                                                  const EOp storeOp, const std::string& input)
{
    if (input != s_DEFAULT_STRING)
    {
        const auto sourceTextureID = m_RenderGraphRef.GetTextureID(input);
        m_RGPassBaseRef.m_TextureReads.insert(sourceTextureID);

        const auto textureID = m_RenderGraphRef.AliasTexture(name, input);
        m_RGPassBaseRef.m_TextureWrites.insert(textureID);

        m_RenderGraphRef.GetRGTexture(textureID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
        m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);

        m_RGPassBaseRef.m_RenderTargetsInfo.emplace_back(clearValue, textureID, loadOp, storeOp);
        m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET;
        return textureID;
    }

    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET;

    m_RGPassBaseRef.m_RenderTargetsInfo.emplace_back(clearValue, textureID, loadOp, storeOp);
    return textureID;
}

RGTextureID RenderGraphBuilder::WriteDepthStencil(const std::string& name, const DepthStencilClearValue& clearValue, const EOp depthLoadOp,
                                                  const EOp depthStoreOp, const EOp stencilLoadOp, const EOp stencilStoreOp,
                                                  const std::string& input)
{
    if (input != s_DEFAULT_STRING)
    {
        const auto sourceTextureID = m_RenderGraphRef.GetTextureID(input);
        m_RGPassBaseRef.m_TextureReads.insert(sourceTextureID);

        const auto textureID = m_RenderGraphRef.AliasTexture(name, input);
        m_RGPassBaseRef.m_TextureWrites.insert(textureID);

        m_RenderGraphRef.GetRGTexture(textureID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
        m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);

        m_RGPassBaseRef.m_DepthStencil = {.ClearValue         = clearValue,
                                          .DepthStencilHandle = textureID,
                                          .DepthLoadOp        = depthLoadOp,
                                          .DepthStoreOp       = depthStoreOp,
                                          .StencilLoadOp      = stencilLoadOp,
                                          .StencilStoreOp     = stencilStoreOp};

        m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET;

        return textureID;
    }

    const auto textureID = m_RenderGraphRef.GetTextureID(name);
    m_RGPassBaseRef.m_TextureWrites.insert(textureID);

    m_RenderGraphRef.GetRGTexture(textureID)->WritePasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_DepthStencil                            = {.ClearValue         = clearValue,
                                                                 .DepthStencilHandle = textureID,
                                                                 .DepthLoadOp        = depthLoadOp,
                                                                 .DepthStoreOp       = depthStoreOp,
                                                                 .StencilLoadOp      = stencilLoadOp,
                                                                 .StencilStoreOp     = stencilStoreOp};
    m_RGPassBaseRef.m_TextureStateMap[textureID.m_ID.value()] = EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET;

    return textureID;
}

RGBufferID RenderGraphBuilder::ReadBuffer(const std::string& name, const ResourceStateFlags resourceStateFlags)
{
    PFR_ASSERT(resourceStateFlags != 0, "Resource state flags can't be empty!");

    const auto bufferID = m_RenderGraphRef.GetBufferID(name);
    m_RGPassBaseRef.m_BufferReads.insert(bufferID);

    m_RenderGraphRef.GetRGBuffer(bufferID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_BufferStateMap[bufferID.m_ID.value()] |= EResourceState::RESOURCE_STATE_STORAGE_BUFFER | resourceStateFlags;
    return bufferID;
}

RGBufferID RenderGraphBuilder::WriteBuffer(const std::string& name, const std::string& input)
{
    if (input != s_DEFAULT_STRING)
    {
        const auto sourceBufferID = m_RenderGraphRef.GetBufferID(input);
        m_RGPassBaseRef.m_BufferReads.insert(sourceBufferID);

        const auto bufferID = m_RenderGraphRef.AliasBuffer(name, input);
        m_RGPassBaseRef.m_BufferWrites.insert(bufferID);

        m_RenderGraphRef.GetRGBuffer(bufferID)->ReadPasses.insert(m_RGPassBaseRef.m_ID);
        m_RenderGraphRef.GetRGBuffer(bufferID)->WritePasses.insert(m_RGPassBaseRef.m_ID);

        m_RGPassBaseRef.m_BufferStateMap[bufferID.m_ID.value()] =
            EResourceState::RESOURCE_STATE_STORAGE_BUFFER | EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE;
        return bufferID;
    }

    const auto bufferID = m_RenderGraphRef.GetBufferID(name);
    m_RGPassBaseRef.m_BufferWrites.insert(bufferID);

    m_RenderGraphRef.GetRGBuffer(bufferID)->WritePasses.insert(m_RGPassBaseRef.m_ID);
    m_RGPassBaseRef.m_BufferStateMap[bufferID.m_ID.value()] =
        EResourceState::RESOURCE_STATE_STORAGE_BUFFER | EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE;
    return bufferID;
}

}  // namespace Pathfinder