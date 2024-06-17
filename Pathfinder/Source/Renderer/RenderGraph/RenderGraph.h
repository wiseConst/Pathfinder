#pragma once

#include <Core/Core.h>

#include <Renderer/RendererCoreDefines.h>
#include "RenderGraphPass.h"
#include "RenderGraphBuilder.h"

#include "RenderGraphResourcePool.h"

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

#if 0
    void AddTextureAlias(const std::string& name, const RGTextureID textureID)
    {
        PFR_ASSERT(!name.empty(), "Name is empty!");
        PFR_ASSERT(!m_TextureNameIDMap.contains(name), "Wrong name to alias! Already present!");

        m_TextureNameIDMap[name] = textureID;
    }

    void AddBufferAlias(const std::string& name, const RGBufferID bufferID)
    {
        PFR_ASSERT(!name.empty(), "Name is empty!");
        PFR_ASSERT(!m_BufferNameIDMap.contains(name), "Wrong name to alias! Already present!");

        m_BufferNameIDMap[name] = bufferID;
    }
#endif

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

    // NOTE: Testing.
    std::unordered_map<std::string, std::string> m_AliasMap;

    std::vector<Unique<RGPassBase>> m_Passes;
    std::vector<Unique<RGTexture>> m_Textures;
    std::vector<Unique<RGBuffer>> m_Buffers;

    std::vector<uint32_t> m_TopologicallySortedPasses;
    std::vector<std::vector<uint32_t>> m_AdjdacencyLists;

    std::unordered_map<std::string, RGTextureID> m_TextureNameIDMap;
    std::unordered_map<std::string, RGBufferID> m_BufferNameIDMap;

    void BuildAdjacencyLists();
    void TopologicalSort();

    void GraphVizDump();
};

}  // namespace Pathfinder