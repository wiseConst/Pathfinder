#pragma once

#include <Core/Core.h>
#include <Renderer/RendererCoreDefines.h>
#include "RenderGraphContext.h"

namespace Pathfinder
{

class RenderGraph;
class RenderGraphBuilder;
class CommandBuffer;

enum class ERGPassType : uint8_t
{
    RGPASS_TYPE_GRAPHICS,
    RGPASS_TYPE_COMPUTE,
    RGPASS_TYPE_TRANSFER,
    // TODO: RGPASS_TYPE_COMPUTE_ASYNC
    // TODO: RGPASS_TYPE_TRANSFER_ASYNC
};

class RenderGraphPassBase : private Uncopyable, private Unmovable
{
  public:
    RenderGraphPassBase(const std::string& name, const ERGPassType rgPassType) : m_Name(name), m_Type(rgPassType) {}
    virtual ~RenderGraphPassBase() = default;

  protected:
    virtual void Setup(RenderGraphBuilder&)                                 = 0;
    virtual void Execute(RenderGraphContext&, Shared<CommandBuffer>&) const = 0;

    // FORCEINLINE bool IsCulled() const { return m_RefCounter == 0; }

  private:
    std::string m_Name = s_DEFAULT_STRING;
    ERGPassType m_Type = ERGPassType::RGPASS_TYPE_GRAPHICS;
    //   uint32_t m_RefCounter{};
    uint64_t m_ID{};

    friend RenderGraph;
    friend RenderGraphBuilder;

    struct RenderTargetInfo
    {
        ColorClearValue ClearValue;
        RGTextureID RenderTargetHandle{};
        EOp LoadOp{};
        EOp StoreOp{};
    };

    struct DepthStencilInfo
    {
        DepthStencilClearValue ClearValue;
        RGTextureID DepthStencilHandle;
        EOp DepthLoadOp{};
        EOp DepthStoreOp{};
        EOp StencilLoadOp{};
        EOp StencilStoreOp{};
        bool bDepthReadOnly;
    };

    struct ViewportScissorInfo
    {
        uint32_t Width{};
        uint32_t Height{};
        uint32_t OffsetX{};
        uint32_t OffsetY{};
    };

    UnorderedSet<RGTextureID> m_TextureCreates;
    UnorderedSet<RGTextureID> m_TextureReads;
    UnorderedSet<RGTextureID> m_TextureWrites;
    //  UnorderedSet<RGTextureID> m_TextureDestroys;  // In case pass culled.
    UnorderedMap<RGTextureID, ResourceStateFlags> m_TextureStateMap;

    UnorderedSet<RGBufferID> m_BufferCreates;
    UnorderedSet<RGBufferID> m_BufferReads;
    UnorderedSet<RGBufferID> m_BufferWrites;
    // UnorderedSet<RGBufferID> m_BufferDestroys;  // In case pass culled.
    UnorderedMap<RGBufferID, ResourceStateFlags> m_BufferStateMap;

    std::vector<RenderTargetInfo> m_RenderTargetsInfo;
    Optional<DepthStencilInfo> m_DepthStencil           = std::nullopt;
    Optional<ViewportScissorInfo> m_ViewportScissorInfo = std::nullopt;
};
using RGPassBase = RenderGraphPassBase;

template <typename TData> class RenderGraphPass final : public RGPassBase
{
  public:
    using SetupFunc   = std::function<void(TData&, RenderGraphBuilder&)>;
    using ExecuteFunc = std::function<void(const TData&, RenderGraphContext&, Shared<CommandBuffer>&)>;

    NODISCARD FORCEINLINE const auto& GetData() const { return m_Data; }

    RenderGraphPass(const std::string& name, const ERGPassType rgPassType, SetupFunc&& setupFunc, ExecuteFunc&& executeFunc)
        : RGPassBase(name, rgPassType), m_SetupFunc(std::move(setupFunc)), m_ExecuteFunc(std::move(executeFunc))
    {
    }
    ~RenderGraphPass() = default;

  private:
    TData m_Data{};
    SetupFunc m_SetupFunc{};
    ExecuteFunc m_ExecuteFunc{};

    FORCEINLINE void Setup(RenderGraphBuilder& builder) final override
    {
        PFR_ASSERT(m_SetupFunc != nullptr, "m_SetupFunc isn't present!");
        m_SetupFunc(m_Data, builder);
    }

    FORCEINLINE void Execute(RenderGraphContext& context, Shared<CommandBuffer>& cb) const final override
    {
        PFR_ASSERT(m_ExecuteFunc != nullptr, "m_ExecuteFunc isn't present!");
        m_ExecuteFunc(m_Data, context, cb);
    }
};

template <> class RenderGraphPass<void> final : public RGPassBase
{
  public:
    using SetupFunc   = std::function<void(RenderGraphBuilder&)>;
    using ExecuteFunc = std::function<void(RenderGraphContext&, Shared<CommandBuffer>&)>;

    RenderGraphPass(const std::string& name, const ERGPassType rgPassType, SetupFunc&& setupFunc, ExecuteFunc&& executeFunc)
        : RGPassBase(name, rgPassType), m_SetupFunc(std::move(setupFunc)), m_ExecuteFunc(std::move(executeFunc))
    {
    }
    ~RenderGraphPass() = default;

    FORCEINLINE void GetData() const { return; }

  private:
    SetupFunc m_SetupFunc{};
    ExecuteFunc m_ExecuteFunc{};

    FORCEINLINE void Setup(RenderGraphBuilder& builder) final override
    {
        PFR_ASSERT(m_SetupFunc != nullptr, "m_SetupFunc isn't present!");
        m_SetupFunc(builder);
    }

    FORCEINLINE void Execute(RenderGraphContext& context, Shared<CommandBuffer>& cb) const final override
    {
        PFR_ASSERT(m_ExecuteFunc != nullptr, "m_ExecuteFunc isn't present!");
        m_ExecuteFunc(context, cb);
    }
};
template <typename TData> using RGPass = RenderGraphPass<TData>;

NODISCARD FORCEINLINE std::string_view RGPassTypeToString(const ERGPassType rgPassType)
{
    switch (rgPassType)
    {
        case ERGPassType::RGPASS_TYPE_GRAPHICS: return "GRAPHICS";
        case ERGPassType::RGPASS_TYPE_COMPUTE:
            return "COMPUTE";
            //  case ERGPassType::RGPASS_TYPE_COMPUTE_ASYNC: return "COMPUTE_ASYNC";
        case ERGPassType::RGPASS_TYPE_TRANSFER: return "TRANSFER";
    }

    PFR_ASSERT(false, "Unknown rg pass type!");
    return s_DEFAULT_STRING;
}

}  // namespace Pathfinder