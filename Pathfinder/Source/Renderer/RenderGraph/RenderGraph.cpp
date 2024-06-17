#include <PathfinderPCH.h>
#include "RenderGraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/CommandBuffer.h>
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

#include <Core/Application.h>

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

    // TODO: Instead of aliasing, create new rgtexture and set its handle
    // TODO: THING ABOVE DONE, but how do I retrieve the actual handle which we get on texture creation?(simply get by aliasmap)

    std::vector<uint32_t> syncedPasses;
    for (auto currPassIdx : m_TopologicallySortedPasses)
    {
        Timer t        = {};
        auto& currPass = m_Passes.at(currPassIdx);
        //        LOG_INFO("Running {}", currPass->m_Name);

        for (auto textureID : currPass->m_TextureCreates)
        {
            PFR_ASSERT(textureID.m_ID.has_value(), "TextureID doesn't have id!");
            auto& rgTexture   = GetRGTexture(textureID);
            rgTexture->Handle = m_ResourcePool.AllocateTexture(rgTexture->Description);
        }

        for (auto bufferID : currPass->m_BufferCreates)
        {
            PFR_ASSERT(bufferID.m_ID.has_value(), "BufferID doesn't have id!");
            auto& rgBuffer   = GetRGBuffer(bufferID);
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

        cb->BeginDebugLabel(currPass->m_Name.data(), stringToVec3(currPass->m_Name));
        std::vector<BufferMemoryBarrier> bufferMemoryBarriers;

        // We want to write, so sync it with prev write/read.
        // TODO:
        for (const auto resourceID : currPass->m_BufferWrites)
        {
            auto& buffer = m_Buffers.at(resourceID.m_ID.value());
            if (!buffer->Handle)  // Means it's alias
            {
                const auto sourceBufferID = GetBufferID(m_AliasMap[buffer->Name]);
                buffer->Handle            = m_Buffers.at(sourceBufferID.m_ID.value())->Handle;
            }

            if (buffer->Handle->GetSpecification().Capacity == 0) continue;

            Optional<uint32_t> prevPassIdx = std::nullopt;
            for (const auto writePassIdx : buffer->WritePasses)
            {
                if (buffer->AlreadySyncedWith.contains(writePassIdx)) continue;

                buffer->AlreadySyncedWith.insert(writePassIdx);
                prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
                break;
            }

            if (!prevPassIdx.has_value()) continue;

            auto& bufferBarrier  = bufferMemoryBarriers.emplace_back(buffer->Handle);
            const auto& prevPass = m_Passes.at(prevPassIdx.value());

            switch (prevPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    PFR_ASSERT(false, "Write to storage buffer from graphics stages??");
                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }

            //     const auto currResourceState = currPass->m_BufferStateMap[resourceID.m_ID.value()];
            switch (currPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    const auto& bufferUsage = buffer->Handle->GetSpecification().UsageFlags;

                    // if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT))
                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_INDIRECT))
                    {
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT;
                        bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                    }

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_VERTEX))
                    {  // what if i don't actually use it as vertex?(pvp)

                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                        bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT;
                    }

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_INDEX))
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_INDEX_READ_BIT;

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_STORAGE))
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;

                    // NOTE: Can be tightly corrected via resource state.
                    bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                  EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
                                                  EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT;

                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }
        }

        // We want to read, so sync it with previous write.
        for (const auto resourceID : currPass->m_BufferReads)
        {
            auto& buffer = m_Buffers.at(resourceID.m_ID.value());
            if (!buffer->Handle)  // Means it's alias
            {
                const auto sourceBufferID = GetBufferID(m_AliasMap[buffer->Name]);
                buffer->Handle            = m_Buffers.at(sourceBufferID.m_ID.value())->Handle;
            }

            if (buffer->Handle->GetSpecification().Capacity == 0) continue;
            Optional<uint32_t> prevPassIdx = std::nullopt;
            for (const auto writePassIdx : buffer->WritePasses)
            {
                if (buffer->AlreadySyncedWith.contains(writePassIdx)) continue;

                buffer->AlreadySyncedWith.insert(writePassIdx);
                prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
                break;
            }

            if (!prevPassIdx.has_value()) continue;

            auto& bufferBarrier  = bufferMemoryBarriers.emplace_back(buffer->Handle);
            const auto& prevPass = m_Passes.at(prevPassIdx.value());

            switch (prevPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    PFR_ASSERT(false, "Write to storage buffer from graphics stages??");
                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    bufferBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    bufferBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }

            //     const auto currResourceState = currPass->m_BufferStateMap[resourceID.m_ID.value()];
            switch (currPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    const auto& bufferUsage = buffer->Handle->GetSpecification().UsageFlags;

                    // if (RGUtils::ResourceStateContains(currResourceState, EResourceState::RESOURCE_STATE_INDIRECT_ARGUMENT))
                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_INDIRECT))
                    {
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_INDIRECT_COMMAND_READ_BIT;
                        bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                    }

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_VERTEX))
                    {  // what if i don't actually use it as vertex?(pvp)

                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
                        bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_VERTEX_SHADER_BIT;
                    }

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_INDEX))
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_INDEX_READ_BIT;

                    if (RGUtils::BufferUsageContains(bufferUsage, EBufferUsage::BUFFER_USAGE_STORAGE))
                        bufferBarrier.dstAccessMask |= EAccessFlags::ACCESS_SHADER_READ_BIT;

                    // NOTE: Can be tightly corrected via resource state.
                    bufferBarrier.dstStageMask |= EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                  EPipelineStage::PIPELINE_STAGE_MESH_SHADER_BIT |
                                                  EPipelineStage::PIPELINE_STAGE_TASK_SHADER_BIT;

                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    bufferBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    bufferBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }
        }

        std::vector<ImageMemoryBarrier> imageMemoryBarriers;

        // Prepare for sampling from.
        for (const auto resourceID : currPass->m_TextureReads)
        {
            auto& texture = m_Textures.at(resourceID.m_ID.value());
            if (!texture->Handle)  // Means it's alias
            {
                const auto sourceTextureID = GetTextureID(m_AliasMap[texture->Name]);
                texture->Handle            = m_Textures.at(sourceTextureID.m_ID.value())->Handle;
            }

            Optional<uint32_t> prevPassIdx = std::nullopt;
            for (const auto writePassIdx : texture->WritePasses)
            {
                if (texture->AlreadySyncedWith.contains(writePassIdx)) continue;

                texture->AlreadySyncedWith.insert(writePassIdx);
                prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
                break;
            }

            if (!prevPassIdx.has_value()) continue;

            // if Write->Write continue(will be synced later)
            if (currPass->m_TextureWrites.contains(resourceID)) continue;

            auto& imageBarrier = imageMemoryBarriers.emplace_back();
            imageBarrier.image = texture->Handle->GetImage();

            const auto& imageSpec  = imageBarrier.image->GetSpecification();
            imageBarrier.oldLayout = imageSpec.Layout;

            imageBarrier.subresourceRange = {
                .baseMipLevel   = 0,
                .mipCount       = imageSpec.Mips,
                .baseArrayLayer = 0,
                .layerCount     = imageSpec.Layers,
            };

            const auto& prevPass = m_Passes.at(prevPassIdx.value());
            switch (prevPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {

                    if (ImageUtils::IsDepthFormat(imageSpec.Format))
                    {
                        imageBarrier.srcStageMask = EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                    EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

                        imageBarrier.srcAccessMask = EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                                     EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                    }
                    else
                    {
                        imageBarrier.srcStageMask = EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        imageBarrier.srcAccessMask =
                            EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    }

                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    imageBarrier.srcAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    imageBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }

            switch (currPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    imageBarrier.dstAccessMask = EAccessFlags::ACCESS_SHADER_READ_BIT | EAccessFlags::ACCESS_SHADER_SAMPLED_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL);
                    imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_GENERAL;
                    imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    imageBarrier.dstAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    imageBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }
        }

        // Prepare for rendering/writing in compute.
        for (const auto resourceID : currPass->m_TextureWrites)
        {
            auto& texture = m_Textures.at(resourceID.m_ID.value());
            if (!texture->Handle)  // Means it's alias
            {
                const auto sourceTextureID = GetTextureID(m_AliasMap[texture->Name]);
                texture->Handle            = m_Textures.at(sourceTextureID.m_ID.value())->Handle;
            }

            Optional<uint32_t> prevPassIdx = std::nullopt;
            for (const auto writePassIdx : texture->WritePasses)
            {
                if (texture->AlreadySyncedWith.contains(writePassIdx)) continue;

                texture->AlreadySyncedWith.insert(writePassIdx);
                prevPassIdx = MakeOptional<uint32_t>(writePassIdx);
                break;
            }

            if (!prevPassIdx.has_value()) continue;

            auto& imageBarrier = imageMemoryBarriers.emplace_back();
            imageBarrier.image = texture->Handle->GetImage();

            const auto& imageSpec       = imageBarrier.image->GetSpecification();
            const bool bTextureCreation = currPass->m_TextureCreates.contains(resourceID);
            imageBarrier.oldLayout      = bTextureCreation ? EImageLayout::IMAGE_LAYOUT_UNDEFINED : imageSpec.Layout;

            imageBarrier.subresourceRange = {
                .baseMipLevel   = 0,
                .mipCount       = imageSpec.Mips,
                .baseArrayLayer = 0,
                .layerCount     = imageSpec.Layers,
            };

            const auto& prevPass = m_Passes.at(prevPassIdx.value());

            if (bTextureCreation)
            {
                imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                imageBarrier.srcAccessMask = EAccessFlags::ACCESS_NONE;
            }
            else
            {
                switch (prevPass->m_Type)
                {
                    case ERGPassType::RGPASS_TYPE_GRAPHICS:
                    {
                        if (ImageUtils::IsDepthFormat(imageSpec.Format))
                        {
                            imageBarrier.srcStageMask = EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                        EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

                            imageBarrier.srcAccessMask = EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                                         EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                        }
                        else
                        {
                            imageBarrier.srcStageMask = EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                            imageBarrier.srcAccessMask =
                                EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
                        }

                        break;
                    }
                    case ERGPassType::RGPASS_TYPE_COMPUTE:
                    {
                        imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                        imageBarrier.srcAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                        break;
                    }
                    case ERGPassType::RGPASS_TYPE_TRANSFER:
                    {
                        imageBarrier.srcStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                        imageBarrier.srcAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                        break;
                    }
                }
            }

            switch (currPass->m_Type)
            {
                case ERGPassType::RGPASS_TYPE_GRAPHICS:
                {
                    if (ImageUtils::IsDepthFormat(imageSpec.Format))
                    {
                        imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                        imageBarrier.newLayout    = EImageLayout::IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                        imageBarrier.dstStageMask = EPipelineStage::PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                    EPipelineStage::PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

                        imageBarrier.dstAccessMask = EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                                     EAccessFlags::ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                    }
                    else
                    {
                        imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                        imageBarrier.newLayout    = EImageLayout::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                        imageBarrier.dstStageMask = EPipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        imageBarrier.dstAccessMask =
                            EAccessFlags::ACCESS_COLOR_ATTACHMENT_WRITE_BIT | EAccessFlags::ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    }
                    break;
                }
                case ERGPassType::RGPASS_TYPE_COMPUTE:
                {
                    imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_GENERAL);
                    imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_GENERAL;
                    imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                    imageBarrier.dstAccessMask = EAccessFlags::ACCESS_SHADER_WRITE_BIT | EAccessFlags::ACCESS_SHADER_READ_BIT;
                    break;
                }
                case ERGPassType::RGPASS_TYPE_TRANSFER:
                {
                    imageBarrier.image->SetLayout(EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    imageBarrier.newLayout     = EImageLayout::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    imageBarrier.dstStageMask  = EPipelineStage::PIPELINE_STAGE_ALL_TRANSFER_BIT;
                    imageBarrier.dstAccessMask = EAccessFlags::ACCESS_TRANSFER_WRITE_BIT;
                    break;
                }
            }
        }

        if (!bufferMemoryBarriers.empty() || !imageMemoryBarriers.empty())
            cb->InsertBarriers({}, bufferMemoryBarriers, imageMemoryBarriers);

        if (currPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS &&
            (!currPass->m_RenderTargetsInfo.empty() || currPass->m_DepthStencil.has_value()))
        {
            PFR_ASSERT(currPass->m_ViewportScissorInfo.has_value(), "RenderPass viewport size is invalid!");

            std::vector<Shared<Texture>> attachments;
            std::vector<RenderingInfo> renderingInfos;
            for (auto& renderTargetInfo : currPass->m_RenderTargetsInfo)
            {
                attachments.emplace_back(GetRGTexture(renderTargetInfo.RenderTargetHandle)->Handle);
                auto& renderingInfo =
                    renderingInfos.emplace_back(renderTargetInfo.ClearValue, renderTargetInfo.LoadOp, renderTargetInfo.StoreOp);
            }

            if (currPass->m_DepthStencil.has_value())
            {
                auto& depthStencilInfo = currPass->m_DepthStencil.value();
                attachments.emplace_back(GetRGTexture(depthStencilInfo.DepthStencilHandle)->Handle);

                // TODO: Stencil
                renderingInfos.emplace_back(depthStencilInfo.ClearValue, depthStencilInfo.DepthLoadOp, depthStencilInfo.DepthStoreOp);
            }

            cb->BeginRendering(attachments, renderingInfos);

            auto& vs = currPass->m_ViewportScissorInfo.value();
            cb->SetViewportAndScissor(vs.Width, vs.Height, vs.OffsetX, vs.OffsetY);
        }

        RenderGraphContext context(*this, *currPass);
        currPass->Execute(context, cb);

        if (currPass->m_Type == ERGPassType::RGPASS_TYPE_GRAPHICS &&
            (!currPass->m_RenderTargetsInfo.empty() || currPass->m_DepthStencil.has_value()))
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
    PFR_ASSERT(m_TextureNameIDMap.find(source) != m_TextureNameIDMap.end(), "Texture with source name isn't declared");
    const auto& srcRGTexture = m_Textures.at(m_TextureNameIDMap.at(source).m_ID.value());

    PFR_ASSERT(!m_AliasMap.contains(name), "Alias with this name to source texture already contains!");
    m_AliasMap[name] = source;

    auto newDesc      = srcRGTexture->Description;
    newDesc.DebugName = name;
    m_Textures.emplace_back(MakeUnique<RGTexture>(m_Textures.size(), newDesc, name));

    const RGTextureID textureID{m_Textures.size() - 1};
    m_TextureNameIDMap[name] = textureID;
    return textureID;
}

RGBufferID RenderGraph::AliasBuffer(const std::string& name, const std::string& source)
{
    PFR_ASSERT(m_BufferNameIDMap.find(source) != m_BufferNameIDMap.end(), "Texture with source name isn't declared");
    const auto& srcRGBuffer = m_Buffers.at(m_BufferNameIDMap.at(source).m_ID.value());

    PFR_ASSERT(!m_AliasMap.contains(name), "Alias with this name to source buffer already contains!");
    m_AliasMap[name] = source;

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

    /*LOG_INFO("After TopologicalSort:");
    for (const auto passIdx : m_TopologicallySortedPasses)
    {
        LOG_INFO("Pass - {}", m_Passes.at(passIdx)->m_Name);
    }*/
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