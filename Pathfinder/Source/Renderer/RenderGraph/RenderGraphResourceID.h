#pragma once

#include <compare>
#include <Core/Core.h>
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

struct RGTextureSpecification
{
    std::string DebugName      = s_DEFAULT_STRING;
    uint32_t Width             = 1;
    uint32_t Height            = 1;
    bool bGenerateMips         = false;
    ESamplerWrap Wrap          = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter      = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    EImageFormat Format        = EImageFormat::FORMAT_RGBA8_UNORM;
    ImageUsageFlags UsageFlags = EImageUsage::IMAGE_USAGE_SAMPLED_BIT;
    uint32_t Layers            = 1;
    const bool bPerFrame       = false;
};

struct RGBufferSpecification
{
    std::string DebugName       = s_DEFAULT_STRING;
    BufferUsageFlags UsageFlags = 0;
    bool bMapPersistent         = false;  // In case it's HOST_VISIBLE, or I force it to be it.
    const bool bPerFrame        = false;
    size_t Capacity             = 0;
};

enum class ERGResourceType : uint8_t
{
    RGRESOURCE_TYPE_BUFFER,
    RGRESOURCE_TYPE_TEXTURE
};

struct RenderGraphResourceID
{
    RenderGraphResourceID()                             = default;
    RenderGraphResourceID(const RenderGraphResourceID&) = default;
    RenderGraphResourceID(const uint64_t id, const bool bIsPerFrame) : m_ID(static_cast<uint32_t>(id)), bPerFramed(bIsPerFrame) {}

    FORCEINLINE void Invalidate() { m_ID = std::nullopt; }
    FORCEINLINE auto IsPerFrame() const noexcept { return bPerFramed; }
    FORCEINLINE auto IsValid() const noexcept { return m_ID.has_value(); }
    auto operator<=>(const RenderGraphResourceID&) const = default;

    std::optional<uint32_t> m_ID = std::nullopt;
    bool bPerFramed              = false;
};
using RGResourceID = RenderGraphResourceID;

template <ERGResourceType TResourceType> struct TypedRenderGraphResourceID : RGResourceID
{
    using RGResourceID::RGResourceID;
};

using RGBufferID  = TypedRenderGraphResourceID<ERGResourceType::RGRESOURCE_TYPE_BUFFER>;
using RGTextureID = TypedRenderGraphResourceID<ERGResourceType::RGRESOURCE_TYPE_TEXTURE>;

}  // namespace Pathfinder

namespace std
{
template <> struct hash<Pathfinder::RGTextureID>
{
    uint64_t operator()(const Pathfinder::RGTextureID& h) const { return hash<decltype(h.m_ID)>()(h.m_ID); }
};

template <> struct hash<Pathfinder::RGBufferID>
{
    uint64_t operator()(const Pathfinder::RGBufferID& h) const { return hash<decltype(h.m_ID)>()(h.m_ID); }
};

}  // namespace std