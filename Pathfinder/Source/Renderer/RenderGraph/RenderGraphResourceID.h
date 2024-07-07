#pragma once

#include <compare>
#include <Core/Core.h>
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

struct RGTextureSpecification
{
    std::string DebugName = s_DEFAULT_STRING;
    glm::uvec3 Dimensions{0};
    bool bGenerateMips         = false;
    ESamplerWrap WrapS         = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerWrap WrapT         = WrapS;
    ESamplerFilter MinFilter   = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    ESamplerFilter MagFilter   = MinFilter;
    EImageFormat Format        = EImageFormat::FORMAT_RGBA8_UNORM;
    ImageUsageFlags UsageFlags = EImageUsage::IMAGE_USAGE_SAMPLED_BIT;
    uint32_t Layers            = 1;
    const bool bPerFrame       = false;
};

struct RGBufferSpecification
{
    std::string DebugName       = s_DEFAULT_STRING;
    BufferFlags ExtraFlags      = 0;
    BufferUsageFlags UsageFlags = 0;
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
    RenderGraphResourceID(const uint64_t id) : m_ID(static_cast<uint32_t>(id)) {}

    FORCEINLINE void Invalidate() { m_ID = std::nullopt; }
    FORCEINLINE auto IsValid() const noexcept { return m_ID.has_value(); }
    auto operator<=>(const RenderGraphResourceID&) const = default;

    std::optional<uint32_t> m_ID = std::nullopt;
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
    uint64_t operator()(const Pathfinder::RGTextureID& h) const { return hash<uint32_t>()(h.m_ID.value()); }
};

template <> struct hash<Pathfinder::RGBufferID>
{
    uint64_t operator()(const Pathfinder::RGBufferID& h) const { return hash<uint32_t>()(h.m_ID.value()); }
};

}  // namespace std