#include <PathfinderPCH.h>
#include "RenderGraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

#include <Core/Application.h>

namespace Pathfinder
{

#define RG_LOG_DEBUG_INFO 0
#define RG_LOG_TOPSORT_RESULT 0

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

FORCEINLINE static bool ResourceStateContains(const ResourceStateFlags source, const EResourceState flag)
{
    return (source & flag) == flag;
}

FORCEINLINE static bool BufferUsageContains(const BufferUsageFlags source, const EBufferUsage flag)
{
    return (source & flag) == flag;
}

FORCEINLINE static bool ImageUsageContains(const ImageUsageFlags source, const EBufferUsage flag)
{
    return (source & flag) == flag;
}

}  // namespace RGUtils

RenderGraph::RenderGraph(const uint8_t currentFrameIndex, const std::string& name, RenderGraphResourcePool& resourcePool)
    : m_CurrentFrameIndex(currentFrameIndex), m_Name(name), m_ResourcePool(resourcePool)
{
}

void RenderGraph::Build()
{
    Timer t = {};

    BuildAdjacencyLists();
    TopologicalSort();
    GraphVizDump();

    LOG_INFO("{} - {:.3f}ms", __FUNCTION__, t.GetElapsedMilliseconds());
}

void RenderGraph::Execute()
{
    auto& rd            = Renderer::GetRendererData();
    m_CurrentFrameIndex = rd->FrameIndex;

    PFR_ASSERT(m_CurrentFrameIndex < s_FRAMES_IN_FLIGHT, "Invalid fif index!");
    m_ResourcePool.Tick();

    constexpr auto stringToVec3 = [](const std::string& str)
    {
        const auto hashCode = std::hash<std::string>{}(str);

        const auto x = static_cast<float>((hashCode & 0xFF0000) >> 16) / 255.0f;
        const auto y = static_cast<float>((hashCode & 0x00FF00) >> 8) / 255.0f;
        const auto z = static_cast<float>(hashCode & 0x0000FF) / 255.0f;

        return glm::vec3(x, y, z);
    };

    std::unordered_set<uint32_t> runPasses;
    for (auto currPassIdx : m_TopologicallySortedPasses)
    {
        Timer t           = {};
        auto& currentPass = m_Passes.at(currPassIdx);

#if RG_LOG_DEBUG_INFO
        LOG_INFO("Running {}", currentPass->m_Name);
#endif

        runPasses.insert(currPassIdx);

        for (auto textureID : currentPass->m_TextureCreates)
        {
            PFR_ASSERT(textureID.m_ID.has_value(), "TextureID doesn't have id!");
            auto& rgTexture   = GetRGTexture(textureID);
            rgTexture->Handle = m_ResourcePool.AllocateTexture(rgTexture->Description);
        }

        for (auto bufferID : currentPass->m_BufferCreates)
        {
            PFR_ASSERT(bufferID.m_ID.has_value(), "BufferID doesn't have id!");
            auto& rgBuffer   = GetRGBuffer(bufferID);
            rgBuffer->Handle = m_ResourcePool.AllocateBuffer(rgBuffer->Description);
        }

        Shared<CommandBuffer> cb = nullptr;
        switch (currentPass->m_Type)
        {
            case ERGPassType::RGPASS_TYPE_GRAPHICS:
            case ERGPassType::RGPASS_TYPE_TRANSFER:
            case ERGPassType::RGPASS_TYPE_COMPUTE: cb = rd->RenderCommandBuffer.at(m_CurrentFrameIndex); break;
        }
        PFR_ASSERT(cb, "Command buffer is not valid!");

        std::vector<BufferMemoryBarrier> bufferMemoryBarriers;

        const auto bufferRAWBarriers = BuildBufferRAWBarriers(currentPass, runPasses);
        bufferMemoryBarriers.insert(bufferMemoryBarriers.end(), bufferRAWBarriers.begin(), bufferRAWBarriers.end());

        const auto bufferWARBarriers = BuildBufferWARBarriers(currentPass, runPasses);
        bufferMemoryBarriers.insert(bufferMemoryBarriers.end(), bufferWARBarriers.begin(), bufferWARBarriers.end());

        const auto bufferWAWBarriers = BuildBufferWAWBarriers(currentPass, runPasses);
        bufferMemoryBarriers.insert(bufferMemoryBarriers.end(), bufferWAWBarriers.begin(), bufferWAWBarriers.end());

        std::vector<ImageMemoryBarrier> imageMemoryBarriers;

        const auto textureRAWBarriers = BuildTextureRAWBarriers(currentPass, runPasses);
        imageMemoryBarriers.insert(imageMemoryBarriers.end(), textureRAWBarriers.begin(), textureRAWBarriers.end());

        const auto textureWARBarriers = BuildTextureWARBarriers(currentPass, runPasses);
        imageMemoryBarriers.insert(imageMemoryBarriers.end(), textureWARBarriers.begin(), textureWARBarriers.end());

        const auto textureWAWBarriers = BuildTextureWAWBarriers(currentPass, runPasses);
        imageMemoryBarriers.insert(imageMemoryBarriers.end(), textureWAWBarriers.begin(), textureWAWBarriers.end());

        cb->BeginDebugLabel(currentPass->m_Name.data(), stringToVec3(currentPass->m_Name));
        if (!bufferMemoryBarriers.empty() || !imageMemoryBarriers.empty())
            cb->InsertBarriers({}, bufferMemoryBarriers, imageMemoryBarriers);

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS &&
            (!currentPass->m_RenderTargetsInfo.empty() || currentPass->m_DepthStencil.has_value()))
        {
            PFR_ASSERT(currentPass->m_ViewportScissorInfo.has_value(), "RenderPass viewport size is invalid!");

            std::vector<Shared<Texture>> attachments;
            std::vector<RenderingInfo> renderingInfos;
            for (auto& renderTargetInfo : currentPass->m_RenderTargetsInfo)
            {
                attachments.emplace_back(GetRGTexture(renderTargetInfo.RenderTargetHandle)->Handle);
                auto& renderingInfo =
                    renderingInfos.emplace_back(renderTargetInfo.ClearValue, renderTargetInfo.LoadOp, renderTargetInfo.StoreOp);
            }

            if (currentPass->m_DepthStencil.has_value())
            {
                auto& depthStencilInfo = currentPass->m_DepthStencil.value();
                attachments.emplace_back(GetRGTexture(depthStencilInfo.DepthStencilHandle)->Handle);

                // TODO: Stencil
                renderingInfos.emplace_back(depthStencilInfo.ClearValue, depthStencilInfo.DepthLoadOp, depthStencilInfo.DepthStoreOp);
            }

            cb->BeginRendering(attachments, renderingInfos);

            auto& vs = currentPass->m_ViewportScissorInfo.value();
            cb->SetViewportAndScissor(vs.Width, vs.Height, vs.OffsetX, vs.OffsetY);
        }

        RenderGraphContext context(*this, *currentPass);
        currentPass->Execute(context, cb);

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS &&
            (!currentPass->m_RenderTargetsInfo.empty() || currentPass->m_DepthStencil.has_value()))
            cb->EndRendering();
        // LOG_INFO("Pass - {}, taken {:.3f}ms CPU time.", currPass->m_Name, t.GetElapsedMilliseconds());

        cb->EndDebugLabel();
    }
}

RGTextureID RenderGraph::DeclareTexture(const std::string& name, const RGTextureSpecification& rgTextureSpec)
{
    PFR_ASSERT(m_TextureNameIDMap.find(name) == m_TextureNameIDMap.end(), "Texture with that name has already been declared");
    m_Textures.emplace_back(MakeUnique<RGTexture>(m_Textures.size(), rgTextureSpec, name));

    const RGTextureID textureID{m_Textures.size() - 1};
    m_TextureNameIDMap[name] = textureID;
    return textureID;
}

RGBufferID RenderGraph::DeclareBuffer(const std::string& name, const RGBufferSpecification& rgBufferSpec)
{
    PFR_ASSERT(m_BufferNameIDMap.find(name) == m_BufferNameIDMap.end(), "Buffer with that name has already been declared");
    m_Buffers.emplace_back(MakeUnique<RGBuffer>(m_Buffers.size(), rgBufferSpec, name));

    const RGBufferID bufferID{m_Buffers.size() - 1};
    m_BufferNameIDMap[name] = bufferID;
    return bufferID;
}

RGTextureID RenderGraph::AliasTexture(const std::string& name, const std::string& source)
{
    PFR_ASSERT(!m_AliasMap.contains(name), "Alias with this name to source texture already contains!");
    PFR_ASSERT(m_TextureNameIDMap.find(source) != m_TextureNameIDMap.end(), "Texture with source name isn't declared");

    const auto& srcRGTexture = m_Textures.at(m_TextureNameIDMap.at(source).m_ID.value());
    m_AliasMap[name]         = source;

    auto newDesc      = srcRGTexture->Description;
    newDesc.DebugName = name;
    m_Textures.emplace_back(MakeUnique<RGTexture>(m_Textures.size(), newDesc, name));

    const RGTextureID textureID{m_Textures.size() - 1};
    m_TextureNameIDMap[name] = textureID;
    return textureID;
}

RGBufferID RenderGraph::AliasBuffer(const std::string& name, const std::string& source)
{
    PFR_ASSERT(!m_AliasMap.contains(name), "Alias with this name to source buffer already contains!");
    PFR_ASSERT(m_BufferNameIDMap.find(source) != m_BufferNameIDMap.end(), "Texture with source name isn't declared");

    const auto& srcRGBuffer = m_Buffers.at(m_BufferNameIDMap.at(source).m_ID.value());
    m_AliasMap[name]        = source;

    auto newDesc      = srcRGBuffer->Description;
    newDesc.DebugName = name;
    m_Buffers.emplace_back(MakeUnique<RGBuffer>(m_Buffers.size(), newDesc, name));

    const RGBufferID bufferID{m_Buffers.size() - 1};
    m_BufferNameIDMap[name] = bufferID;
    return bufferID;
}

NODISCARD Unique<RGTexture>& RenderGraph::GetRGTexture(const RGTextureID resourceID)
{
    return m_Textures.at(resourceID.m_ID.value());
}

NODISCARD Unique<RGBuffer>& RenderGraph::GetRGBuffer(const RGBufferID resourceID)
{
    return m_Buffers.at(resourceID.m_ID.value());
}

NODISCARD Shared<Texture>& RenderGraph::GetTexture(const RGTextureID resourceID)
{
    return GetRGTexture(resourceID)->Handle;
}

NODISCARD Shared<Buffer>& RenderGraph::GetBuffer(const RGBufferID resourceID)
{
    return GetRGBuffer(resourceID)->Handle;
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
            const auto& otherPass = m_Passes.at(otherPassIndex);
            if (otherPass->m_Name == pass->m_Name) continue;

            bool bDepends = false;
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

#if RG_LOG_TOPSORT_RESULT
    LOG_INFO("After TopologicalSort:");
    for (const auto passIdx : m_TopologicallySortedPasses)
    {
        LOG_INFO("Pass - {}", m_Passes.at(passIdx)->m_Name);
    }
#endif
}

std::vector<BufferMemoryBarrier> RenderGraph::BuildBufferRAWBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<BufferMemoryBarrier> bufferMemoryBarriers;

#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_BufferReads)
    {
        auto& buffer = m_Buffers.at(resourceID.m_ID.value());
        if (!buffer->Handle)  // Means it's alias
        {
            const auto sourceBufferID = GetBufferID(m_AliasMap[buffer->Name]);
            buffer->Handle            = m_Buffers.at(sourceBufferID.m_ID.value())->Handle;
        }

        if (buffer->Handle->GetSpecification().Capacity == 0) continue;

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto writePassIdx : buffer->WritePasses)
        {
            if (!runPasses.contains(writePassIdx) || buffer->AlreadySyncedWith.contains(writePassIdx) &&!m_AliasMap.contains(buffer->Name)) continue;

            buffer->AlreadySyncedWith.insert(writePassIdx);
            prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", buffer->Handle->GetSpecification().DebugName);
#endif

        auto& bufferBarrier  = bufferMemoryBarriers.emplace_back(buffer->Handle);
        const auto& prevPass = m_Passes.at(prevPassIdx.value());

        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_BufferStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_BufferStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_BufferStateMap[GetBufferID(m_AliasMap[buffer->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Nothing to do with this.
        // if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_BUFFER)) {  }

        // TODO:
        // if (RGUtils::ResourceStateContains(prevResourceState,EResourceState::RESOURCE_STATE_ACCELERATION_STRUCTURE))

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }

        const auto currResourceState = currentPass->m_BufferStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT))
        {
            bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE))
        {
            bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT | EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
            bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE))
        {
            bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_READ_BIT;  // Maybe RW?
        }
    }

    return bufferMemoryBarriers;
}

std::vector<BufferMemoryBarrier> RenderGraph::BuildBufferWARBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<BufferMemoryBarrier> bufferMemoryBarriers;
#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_BufferWrites)
    {
        auto& buffer = m_Buffers.at(resourceID.m_ID.value());
        if (!buffer->Handle)  // Means it's alias
        {
            const auto sourceBufferID = GetBufferID(m_AliasMap[buffer->Name]);
            buffer->Handle            = m_Buffers.at(sourceBufferID.m_ID.value())->Handle;
        }

        if (buffer->Handle->GetSpecification().Capacity == 0) continue;
      //  if (currentPass->m_BufferReads.contains(resourceID)) continue;  // In case it's RMW buffer

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto readPassIdx : buffer->ReadPasses)
        {
            if (!runPasses.contains(readPassIdx) || buffer->AlreadySyncedWith.contains(readPassIdx)) continue;

            buffer->AlreadySyncedWith.insert(readPassIdx);
            prevPassIdx = MakeOptional<uint32_t>(readPassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", buffer->Handle->GetSpecification().DebugName);
#endif

        auto& bufferBarrier  = bufferMemoryBarriers.emplace_back(buffer->Handle);
        const auto& prevPass = m_Passes.at(prevPassIdx.value());

        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_BufferStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_BufferStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_BufferStateMap[GetBufferID(m_AliasMap[buffer->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Nothing to do with this.
        // if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_BUFFER)) {  }

        // TODO:
        // if (RGUtils::ResourceStateContains(prevResourceState,EResourceState::RESOURCE_STATE_ACCELERATION_STRUCTURE))

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT | EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                                          EPipelineStage::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_READ_BIT;  // Maybe RW?
        }

        const auto currResourceState = currentPass->m_BufferStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }
    }
    return bufferMemoryBarriers;
}

std::vector<BufferMemoryBarrier> RenderGraph::BuildBufferWAWBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<BufferMemoryBarrier> bufferMemoryBarriers;
#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_BufferWrites)
    {
        auto& buffer = m_Buffers.at(resourceID.m_ID.value());
        if (!buffer->Handle)  // Means it's alias
        {
            const auto sourceBufferID = GetBufferID(m_AliasMap[buffer->Name]);
            buffer->Handle            = m_Buffers.at(sourceBufferID.m_ID.value())->Handle;
        }

        if (buffer->Handle->GetSpecification().Capacity == 0) continue;

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto writePassIdx : buffer->WritePasses)
        {
            if (!runPasses.contains(writePassIdx) || buffer->AlreadySyncedWith.contains(writePassIdx)) continue;

            buffer->AlreadySyncedWith.insert(writePassIdx);
            prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", buffer->Handle->GetSpecification().DebugName);
#endif

        auto& bufferBarrier  = bufferMemoryBarriers.emplace_back(buffer->Handle);
        const auto& prevPass = m_Passes.at(prevPassIdx.value());

        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_BufferStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_BufferStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_BufferStateMap[GetBufferID(m_AliasMap[buffer->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Nothing to do with this.
        // if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_BUFFER)) {  }

        // TODO:
        // if (RGUtils::ResourceStateContains(prevResourceState,EResourceState::RESOURCE_STATE_ACCELERATION_STRUCTURE))

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            bufferBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            bufferBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }

        const auto currResourceState = currentPass->m_BufferStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }
    }

    return bufferMemoryBarriers;
}

std::vector<ImageMemoryBarrier> RenderGraph::BuildTextureRAWBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<ImageMemoryBarrier> imageMemoryBarriers;
#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_TextureReads)
    {
        auto& texture = m_Textures.at(resourceID.m_ID.value());
        if (!texture->Handle)  // Means it's alias
        {
            const auto sourceTextureID = GetTextureID(m_AliasMap[texture->Name]);
            texture->Handle            = m_Textures.at(sourceTextureID.m_ID.value())->Handle;
        }

        if (currentPass->m_Type != ERGPassType::RGPASS_TYPE_TRANSFER &&
            texture->Handle->GetImage()->GetSpecification().Layout == EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            continue;

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto writePassIdx : texture->WritePasses)
        {
            if (!runPasses.contains(writePassIdx) || texture->AlreadySyncedWith.contains(writePassIdx)) continue;

            texture->AlreadySyncedWith.insert(writePassIdx);
            prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", texture->Handle->GetSpecification().DebugName);
#endif

        // if Write->Write continue(will be synced later)
        if (currentPass->m_TextureWrites.contains(resourceID)) continue;

        auto& imageBarrier = imageMemoryBarriers.emplace_back();
        imageBarrier.image = texture->Handle->GetImage();

        const auto& imageSpec = imageBarrier.image->GetSpecification();

        imageBarrier.subresourceRange = {
            .baseMipLevel   = 0,
            .mipCount       = imageSpec.Mips,
            .baseArrayLayer = 0,
            .layerCount     = imageSpec.Layers,
        };

        const auto& prevPass = m_Passes.at(prevPassIdx.value());
        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_TextureStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_TextureStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_TextureStateMap[GetTextureID(m_AliasMap[texture->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Maybe remove storage image usage?
        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE) ||
            RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_IMAGE))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_GENERAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |=
                EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.srcAccessMask |=
                EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.oldLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }

        const auto currResourceState = currentPass->m_TextureStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_VERTEX_SHADER_RESOURCE))
        {
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT | EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT |
                                         EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
                                         EPipelineStage::PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
                                         EPipelineStage::PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
                                         EPipelineStage::PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE))
        {
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE))
        {
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
        }

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_READ_BIT;  // Maybe RW?
        }
    }

    return imageMemoryBarriers;
}

std::vector<ImageMemoryBarrier> RenderGraph::BuildTextureWARBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<ImageMemoryBarrier> imageMemoryBarriers;
#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_TextureWrites)
    {
        // NOTE: First resource creation will be handled in BuildTextureWAWBarriers()
        if (currentPass->m_TextureCreates.contains(resourceID)) continue;

        auto& texture = m_Textures.at(resourceID.m_ID.value());
        if (!texture->Handle)  // Means it's alias
        {
            const auto sourceTextureID = GetTextureID(m_AliasMap[texture->Name]);
            texture->Handle            = m_Textures.at(sourceTextureID.m_ID.value())->Handle;
        }

        // Means aliased, will be handled at BuildTextureWAWBarriers()
        if (texture->Name != texture->Handle->GetSpecification().DebugName) continue;

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto readPassIdx : texture->ReadPasses)
        {
            if (!runPasses.contains(readPassIdx) || texture->AlreadySyncedWith.contains(readPassIdx)) continue;

            texture->AlreadySyncedWith.insert(readPassIdx);
            prevPassIdx = MakeOptional<uint32_t>(readPassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", texture->Handle->GetSpecification().DebugName);
#endif
        auto& imageBarrier = imageMemoryBarriers.emplace_back();
        imageBarrier.image = texture->Handle->GetImage();

        const auto& imageSpec         = imageBarrier.image->GetSpecification();
        imageBarrier.subresourceRange = {
            .baseMipLevel   = 0,
            .mipCount       = imageSpec.Mips,
            .baseArrayLayer = 0,
            .layerCount     = imageSpec.Layers,
        };

        const auto& prevPass = m_Passes.at(prevPassIdx.value());
        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_TextureStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_TextureStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_TextureStateMap[GetTextureID(m_AliasMap[texture->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Maybe remove storage image usage?
        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE) ||
            RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_IMAGE))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_GENERAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |=
                EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.oldLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_READ_BIT;  // Maybe RW?
        }

        const auto currResourceState = currentPass->m_TextureStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imageBarrier.dstStageMask |=
                EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.dstAccessMask |=
                EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE) ||
            RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_STORAGE_IMAGE))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_GENERAL;
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_WRITE_BIT;
        }

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }
    }

    return imageMemoryBarriers;
}

std::vector<ImageMemoryBarrier> RenderGraph::BuildTextureWAWBarriers(const Unique<RGPassBase>& currentPass,
                                                                     const std::unordered_set<uint32_t>& runPasses)
{
    std::vector<ImageMemoryBarrier> imageMemoryBarriers;
#if RG_LOG_DEBUG_INFO
    LOG_INFO("\t{}:", __FUNCTION__);
#endif
    for (const auto resourceID : currentPass->m_TextureWrites)
    {
        auto& texture = m_Textures.at(resourceID.m_ID.value());
        if (!texture->Handle)  // Means it's alias
        {
            const auto sourceTextureID = GetTextureID(m_AliasMap[texture->Name]);
            texture->Handle            = m_Textures.at(sourceTextureID.m_ID.value())->Handle;
        }

        // TODO: Maybe insert AlreadySyncedWith in the end?
        Optional<uint32_t> prevPassIdx = std::nullopt;
        for (const auto writePassIdx : texture->WritePasses)
        {
            if (!runPasses.contains(writePassIdx) || texture->AlreadySyncedWith.contains(writePassIdx)) continue;

            texture->AlreadySyncedWith.insert(writePassIdx);
            prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
            break;
        }

        if (!prevPassIdx.has_value()) continue;
#if RG_LOG_DEBUG_INFO
        LOG_INFO("\t\t{}", texture->Handle->GetSpecification().DebugName);
#endif

        auto& imageBarrier = imageMemoryBarriers.emplace_back();
        imageBarrier.image = texture->Handle->GetImage();

        const auto& imageSpec       = imageBarrier.image->GetSpecification();
        const bool bTextureCreation = currentPass->m_TextureCreates.contains(resourceID);

        imageBarrier.subresourceRange = {
            .baseMipLevel   = 0,
            .mipCount       = imageSpec.Mips,
            .baseArrayLayer = 0,
            .layerCount     = imageSpec.Layers,
        };

        const auto& prevPass = m_Passes.at(prevPassIdx.value());
        // Retrieving hard in case resource is aliased.
        const auto prevResourceState = prevPass->m_TextureStateMap.contains(resourceID.m_ID.value())
                                           ? prevPass->m_TextureStateMap[resourceID.m_ID.value()]
                                           : prevPass->m_TextureStateMap[GetTextureID(m_AliasMap[texture->Name]).m_ID.value()];
        PFR_ASSERT(prevResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");

        // NOTE: Maybe remove storage image usage?
        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE) ||
            RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_STORAGE_IMAGE))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_GENERAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(prevResourceState, EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET))
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imageBarrier.srcStageMask |=
                EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.srcAccessMask |=
                EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if (prevPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.oldLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }

        if (bTextureCreation)
        {
            imageBarrier.oldLayout     = bTextureCreation ? EImageLayout::IMAGE_LAYOUT_UNDEFINED : imageSpec.Layout;
            imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            imageBarrier.srcAccessMask = EAccessFlags::ACCESS_NONE;
        }

        // NOTE: Since I drop all the things from other Build*Barriers() here:
        if (imageSpec.Layout == EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            imageBarrier.oldLayout = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageBarrier.srcStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            imageBarrier.srcAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
        }

        const auto currResourceState = currentPass->m_TextureStateMap[resourceID.m_ID.value()];
        PFR_ASSERT(currResourceState != EResourceState::RESOURCE_STATE_UNDEFINED, "Resource state is undefined!");
        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COLOR_RENDER_TARGET))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_DEPTH_RENDER_TARGET))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            imageBarrier.dstStageMask |=
                EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            imageBarrier.dstAccessMask |=
                EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }

        if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_COMPUTE_SHADER_RESOURCE) ||
            RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_STORAGE_IMAGE))
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL);
            imageBarrier.newLayout = EImageLayout::IMAGE_LAYOUT_GENERAL;
            imageBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            imageBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_WRITE_BIT;
        }

        if (currentPass->m_Type == ERGPassType::RGPASS_TYPE_TRANSFER)
        {
            imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
            imageBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;  // Maybe RW?
        }
    }

    return imageMemoryBarriers;
}

void RenderGraph::GraphVizDump()
{
    PFR_ASSERT(!m_Passes.empty() && !m_Name.empty(), "DebugName or passes array is not valid!");

    std::stringstream ss;
    ss << "digraph " << m_Name << " {" << std::endl;
    ss << "\tnode [shape=rectangle, style=filled];" << std::endl;
    ss << "\tedge [color=black];" << std::endl << std::endl;

    for (const auto idx : m_TopologicallySortedPasses)
    {
        const auto& pass = m_Passes.at(idx);
        for (const auto passIndex : m_AdjdacencyLists.at(idx))
        {
            ss << "\t" << pass->m_Name << " -> " << m_Passes.at(passIndex)->m_Name << std::endl;
        }
        ss << std::endl;
    }

    ss << "}" << std::endl;

    const auto& appSpec = Application::Get().GetSpecification();
    const auto savePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / "render_graph_ref.dot";
    SaveData(savePath.string().data(), ss.str().data(), ss.str().size());
}

}  // namespace Pathfinder