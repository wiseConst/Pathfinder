#pragma once

#include <Core/Core.h>
#include "RenderGraphResourceId.h"

namespace Pathfinder
{

class Texture;
class Buffer;

class RenderGraphPassBase;
class RenderGraph;

template <ERGResourceType ResourceType> struct RGResourceTraits;

template <> struct RGResourceTraits<ERGResourceType::RGRESOURCE_TYPE_TEXTURE>
{
    using Resource     = Texture;
    using ResourceDesc = RGTextureSpecification;
};

template <> struct RGResourceTraits<ERGResourceType::RGRESOURCE_TYPE_BUFFER>
{
    using Resource     = Buffer;
    using ResourceDesc = RGBufferSpecification;
};

struct RenderGraphResource
{
    RenderGraphResource(const uint64_t id, const std::string_view& name) : ID(id), Name(name) /*, Version(0), RefCounter(0) */ {}

    EResourceState State = EResourceState::RESOURCE_STATE_COMMON;
    uint64_t ID{};
    // uint64_t Version{};
    // uint64_t RefCounter{};

    // std::optional<uint32_t> WriterPassIdx  = std::nullopt;
    // std::optional<uint32_t> LastUsedByPass = std::nullopt;
    std::string_view Name = s_DEFAULT_STRING;
};
using RGResource = RenderGraphResource;

template <ERGResourceType ResourceType> struct TypedRenderGraphResource : RenderGraphResource
{
    using Resource     = RGResourceTraits<ResourceType>::Resource;
    using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

    // NOTE: Only for imported
    TypedRenderGraphResource(const uint64_t id, Shared<Resource> resource, const std::string_view name = s_DEFAULT_STRING)
        : RenderGraphResource(id, name), Handle(resource), Description(resource->GetSpecification())
    {
    }

    // NOTE: Default use-case(in e.g. declaring)
    TypedRenderGraphResource(const uint64_t id, const ResourceDesc& desc, const std::string_view name = s_DEFAULT_STRING)
        : RenderGraphResource(id, name), Handle(nullptr), Description(desc)
    {
    }

    Shared<Resource> Handle  = nullptr;
    ResourceDesc Description = {};
};

using RGTexture = TypedRenderGraphResource<ERGResourceType::RGRESOURCE_TYPE_TEXTURE>;
using RGBuffer  = TypedRenderGraphResource<ERGResourceType::RGRESOURCE_TYPE_BUFFER>;

class RenderGraphContext final : private Uncopyable, private Unmovable
{
  public:
    NODISCARD Shared<Texture>& GetTexture(const RGTextureID resourceID) const;
    NODISCARD Shared<Buffer>& GetBuffer(const RGBufferID resourceID) const;

  private:
    RenderGraph& m_RenderGraphRef;
    RenderGraphPassBase& m_RGPassBaseRef;

    friend RenderGraph;
    RenderGraphContext() = delete;
    RenderGraphContext(RenderGraph&, RenderGraphPassBase&);
};
using RGContext = RenderGraphContext;

}  // namespace Pathfinder