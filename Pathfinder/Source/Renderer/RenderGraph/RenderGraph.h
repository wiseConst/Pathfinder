#pragma once

#include <Core/Core.h>

#include <Renderer/RendererCoreDefines.h>
#include "RenderGraphPass.h"
#include "RenderGraphBuilder.h"

#include "RenderGraphResourcePool.h"

// https://medium.com/@pavlo.muratov/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3
// NOTE: Heavily inspired by Yuri ODonnel, themaister, Adria-DX12, LegitEngine.

namespace Pathfinder
{

class CommandBuffer;

class RenderGraph final : private Uncopyable, private Unmovable
{
  public:
    RenderGraph(const uint8_t currentFrameIndex, const std::string& name, RenderGraphResourcePool& resourcePool);
    ~RenderGraph() = default;

    template <typename TData, typename... Args>
        requires std::is_constructible_v<RenderGraphPass<TData>, Args...>
    [[maybe_unused]] decltype(auto) AddPass(Args&&... args)
    {
        m_Passes.emplace_back(MakeUnique<RenderGraphPass<TData>>(std::forward<Args>(args)...));
        Unique<RGPassBase>& pass = m_Passes.back();
        pass->m_ID               = m_Passes.size() - 1;
        RenderGraphBuilder builder(*this, *pass);
        pass->Setup(builder);
        return *dynamic_cast<RenderGraphPass<TData>*>(pass.get());
    }

    void Build();
    void Execute();

    RGTextureID DeclareTexture(const std::string& name, const RGTextureSpecification& rgTextureSpec);
    RGBufferID DeclareBuffer(const std::string& name, const RGBufferSpecification& rgBufferSpec);

    RGTextureID AliasTexture(const std::string& name, const std::string& source);
    RGBufferID AliasBuffer(const std::string& name, const std::string& source);

    FORCEINLINE NODISCARD RGTextureID GetTextureID(const std::string& name) const
    {
        PFR_ASSERT(!name.empty(), "Invalid texture name!");
        PFR_ASSERT(m_TextureNameIDMap.contains(name), "Texture isn't declared!");

        return m_TextureNameIDMap.at(name);
    }

    FORCEINLINE NODISCARD RGBufferID GetBufferID(const std::string& name) const
    {
        PFR_ASSERT(!name.empty(), "Invalid buffer name!");
        PFR_ASSERT(m_BufferNameIDMap.contains(name), "Buffer isn't declared!");

        return m_BufferNameIDMap.at(name);
    }

    NODISCARD Shared<Texture>& GetTexture(const RGTextureID resourceID);
    NODISCARD Shared<Buffer>& GetBuffer(const RGBufferID resourceID);

    NODISCARD Unique<RGTexture>& GetRGTexture(const RGTextureID resourceID);
    NODISCARD Unique<RGBuffer>& GetRGBuffer(const RGBufferID resourceID);

  private:
    std::string m_Name = s_DEFAULT_STRING;
    uint8_t m_CurrentFrameIndex{};
    RenderGraphResourcePool& m_ResourcePool;

    std::vector<Unique<RGPassBase>> m_Passes;
    std::vector<Unique<RGTexture>> m_Textures;
    std::vector<Unique<RGBuffer>> m_Buffers;

    std::vector<uint32_t> m_TopologicallySortedPasses;
    std::vector<std::vector<uint32_t>> m_AdjdacencyLists;

    UnorderedMap<std::string, std::string> m_AliasMap;
    UnorderedMap<std::string, RGTextureID> m_TextureNameIDMap;
    UnorderedMap<std::string, RGBufferID> m_BufferNameIDMap;

    void BuildAdjacencyLists();
    void TopologicalSort();

    // TODO: Populate it, cuz it's poor dump rn.
    void GraphVizDump();

    // BUFFER: Read-After-Write
    std::vector<BufferMemoryBarrier> BuildBufferRAWBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);
    // BUFFER: Write-After-Read
    std::vector<BufferMemoryBarrier> BuildBufferWARBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);
    // BUFFER: Write-After-Write
    std::vector<BufferMemoryBarrier> BuildBufferWAWBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);

    // BUFFER: Read-After-Write
    std::vector<ImageMemoryBarrier> BuildTextureRAWBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);
    // TEXTURE: Write-After-Read
    std::vector<ImageMemoryBarrier> BuildTextureWARBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);
    // TEXTURE: Write-After-Write
    std::vector<ImageMemoryBarrier> BuildTextureWAWBarriers(const Unique<RGPassBase>& currentPass, const UnorderedSet<uint32_t>& runPasses);
};

}  // namespace Pathfinder