#include <PathfinderPCH.h>
#include "RenderGraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/CommandBuffer.h>

namespace Pathfinder
{

namespace RGUtils
{

static void DepthFirstSearch(const uint32_t passIndex, std::vector<uint8_t>& visitedPasses,
                             std::vector<uint32_t>& topologicallySortedPasses, const std::vector<std::vector<uint32_t>>& adjdacencyLists)
{
    PFR_ASSERT(passIndex < visitedPasses.size(), "Invalid pass index!");

    visitedPasses[passIndex] = 1;  // Visiting in process.
    for (const auto otherPassIndex : adjdacencyLists.at(passIndex))
    {
        PFR_ASSERT(visitedPasses.at(otherPassIndex) != 1, "Found cycle! RenderGraph is not DAG!");

        if (visitedPasses.at(otherPassIndex) == 0)
            DepthFirstSearch(otherPassIndex, visitedPasses, topologicallySortedPasses, adjdacencyLists);
    }

    topologicallySortedPasses.emplace_back(passIndex);
    visitedPasses[passIndex] = 2;  // Visited at all.
}

}  // namespace RGUtils

void RenderGraph::Build()
{
    Timer t = {};

    BuildAdjacencyLists();
    TopologicalSort();

    LOG_INFO("{} - {:.3f}ms", __FUNCTION__, t.GetElapsedMilliseconds());
}

void RenderGraph::Execute()
{
    auto& rd            = Renderer::GetRendererData();
    m_CurrentFrameIndex = rd->FrameIndex;

    PFR_ASSERT(m_CurrentFrameIndex < s_FRAMES_IN_FLIGHT, "Invalid fif index!");
    m_ResourcePool.Tick();

    for (auto currPassIdx : m_TopologicallySortedPasses)
    {
        auto& currPass = m_Passes.at(currPassIdx);

        for (auto textureID : currPass->m_TextureCreates)
        {
            PFR_ASSERT(textureID.m_ID.has_value(), "TextureID doesn't have id!");
            auto& rgTexture   = m_Textures.at(textureID.m_ID.value());
            rgTexture->Handle = m_ResourcePool.AllocateTexture(rgTexture->Description);
        }

        for (auto bufferID : currPass->m_BufferCreates)
        {
            PFR_ASSERT(bufferID.m_ID.has_value(), "BufferID doesn't have id!");
            auto& rgBuffer   = m_Buffers.at(bufferID.m_ID.value());
            rgBuffer->Handle = m_ResourcePool.AllocateBuffer(rgBuffer->Description);
        }

        Shared<CommandBuffer> cb = nullptr;
        switch (currPass->m_Type)
        {
            case ERGPassType::RGPASS_TYPE_GRAPHICS:
            case ERGPassType::RGPASS_TYPE_TRANSFER:
            case ERGPassType::RGPASS_TYPE_COMPUTE: cb = rd->RenderCommandBuffer.at(m_CurrentFrameIndex); break;
        }
        PFR_ASSERT(cb, "Command buffer is not valid!");

        std::vector<BufferMemoryBarrier> bufferMemoryBarriers;
        for (const auto resourceID : currPass->m_BufferReads)
        {
            auto& buffer = m_Buffers.at(resourceID.m_ID.value());
            // buffer->State
        }

        // RMW:

        std::vector<ImageMemoryBarrier> imageMemoryBarriers;
        /* for (const auto resourceID : currPass->m_TextureReads)
        {
            const bool bIsRMWTexture =
                currPass->m_TextureStateMap[resourceID.m_ID.value()] &
                    (EResourceState::RESOURCE_STATE_NON_FRAGMENT_SHADER_RESOURCE | EResourceState::RESOURCE_STATE_RENDER_TARGET) ||
                currPass->m_TextureStateMap[resourceID.m_ID.value()] &
                    (EResourceState::RESOURCE_STATE_NON_FRAGMENT_SHADER_RESOURCE | EResourceState::RESOURCE_STATE_DEPTH_WRITE);
        }*/

        for (const auto resourceID : currPass->m_TextureWrites)
        {
        }

        if (!bufferMemoryBarriers.empty() || !imageMemoryBarriers.empty())
            cb->InsertBarriers({}, bufferMemoryBarriers, imageMemoryBarriers);

        if (currPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS)
        {
            if (!currPass->m_RenderTargetsInfo.empty() || currPass->m_DepthStencil.has_value())
                PFR_ASSERT(currPass->m_ViewportScissorInfo.has_value(), "RenderPass viewport size is invalid!");
            std::vector<RenderingInfo> renderingInfos;
            // cb->BeginRendering()

            auto& vs = currPass->m_ViewportScissorInfo.value();
            cb->SetViewportAndScissor(vs.Width, vs.Height, vs.OffsetX, vs.OffsetY);
        }

        RenderGraphContext context(*this, *currPass);
        currPass->Execute(context, cb);

        if (currPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS) cb->EndRendering();
    }
}

RGTextureID RenderGraph::DeclareTexture(const std::string& name, const RGTextureSpecification& rgTextureSpec)
{
    PFR_ASSERT(m_TextureNameIDMap.find(name) != m_TextureNameIDMap.end(), "Texture with that name has already been declared");
    m_Textures.emplace_back(MakeUnique<RGTexture>(m_Textures.size(), rgTextureSpec, name));

    const RGTextureID textureID{m_Textures.size() - 1, rgTextureSpec.bPerFrame};
    m_TextureNameIDMap[name] = textureID;
    return textureID;
}

RGBufferID RenderGraph::DeclareBuffer(const std::string& name, const RGBufferSpecification& rgBufferSpec)
{
    PFR_ASSERT(m_BufferNameIDMap.find(name) == m_BufferNameIDMap.end(), "Buffer with that name has already been declared");
    m_Buffers.emplace_back(MakeUnique<RGBuffer>(m_Buffers.size(), rgBufferSpec, name));

    const RGBufferID bufferID{m_Buffers.size() - 1, rgBufferSpec.bPerFrame};
    m_BufferNameIDMap[name] = bufferID;
    return bufferID;
}

Shared<Texture> RenderGraph::GetTexture(const RGTextureID resourceID) const {
    return nullptr;
}

Shared<Buffer> RenderGraph::GetBuffer(const RGBufferID resourceID) const
{
    return nullptr;
}

void RenderGraph::BuildAdjacencyLists()
{
    m_AdjdacencyLists.resize(m_Passes.size());
    for (uint32_t passIndex{}; passIndex < m_Passes.size(); ++passIndex)
    {
        const auto& pass        = m_Passes.at(passIndex);
        auto& passAdjacencyList = m_AdjdacencyLists.at(passIndex);

        for (uint32_t otherPassIndex{}; otherPassIndex < m_Passes.size(); ++otherPassIndex)
        {
            if (otherPassIndex == passIndex) continue;

            bool bDepends         = false;
            const auto& otherPass = m_Passes.at(otherPassIndex);
            for (const auto otherPassInput : otherPass->m_TextureReads)
            {
                if (pass->m_TextureWrites.find(otherPassInput) == pass->m_TextureWrites.end()) continue;

                passAdjacencyList.emplace_back(otherPassIndex);
                bDepends = true;
                break;
            }

            if (bDepends) continue;
            for (const auto otherPassInput : otherPass->m_BufferReads)
            {
                if (pass->m_BufferWrites.find(otherPassInput) == pass->m_BufferWrites.end()) continue;

                passAdjacencyList.emplace_back(otherPassIndex);
                break;
            }
        }
    }
}

void RenderGraph::TopologicalSort()
{
    PFR_ASSERT(!m_Passes.empty() && !m_AdjdacencyLists.empty(), "RenderGraph is invalid!");

    std::vector<uint8_t> visitedPasses(m_Passes.size(), 0);  // Not visited.
    for (uint32_t passIndex{}; passIndex < visitedPasses.size(); ++passIndex)
    {
        if (visitedPasses.at(passIndex) == 0)
            RGUtils::DepthFirstSearch(passIndex, visitedPasses, m_TopologicallySortedPasses, m_AdjdacencyLists);
    }
    std::ranges::reverse(m_TopologicallySortedPasses);
}

}  // namespace Pathfinder