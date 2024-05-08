#ifndef RENDERGRAPH_H
#define RENDERGRAPH_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{

// NOTE: RenderGraph built via this book: Castorina M. Mastering Graphics Programming with Vulkan 2023

enum class ERenderGraphResourceType : uint8_t
{
    RENDER_GRAPH_RESOURCE_TYPE_ATTACHMENT = BIT(0),
    RENDER_GRAPH_RESOURCE_TYPE_TEXTURE    = BIT(1),
    RENDER_GRAPH_RESOURCE_TYPE_BUFFER     = BIT(2),
    RENDER_GRAPH_RESOURCE_TYPE_REFERENCE =
        BIT(3)  // used exclusively to ensure the right edges between nodes are computed without creating a new resource.
};

struct RenderGraphResourceInfo
{
    bool bExternal = false;
};

struct RenderGraphResourceHandle
{
};

struct RenderGraphNode
{
    std::string Name = s_DEFAULT_STRING;
    Shared<Pathfinder::Framebuffer> Framebuffer;
    bool bRenderScene = false;  // false means render fullscreen quad, otherwise render whole scene

    std::vector<RenderGraphResourceHandle> Inputs;
    std::vector<RenderGraphResourceHandle> Outputs;
    std::vector<uint32_t> Edges;  // Index into render graph array of nodes
};

struct RenderGraphResource
{
    ERenderGraphResourceType Type = ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_ATTACHMENT;
    RenderGraphResourceInfo ResourceInfo;

    RenderGraphNode
        Producer;  // Stores a reference to the node that outputs a resource. This will be used to determine the edges of the graph.
    RenderGraphResourceHandle OutputHandle;  // Stores the parent resource.

    uint32_t ReferenceCounter = 0;  // Will be used when computing which resources can be aliased. Aliasing is a technique that allows
                                    // multiple resources to share the same memory.

    std::string Name = s_DEFAULT_STRING;  // Contains the name of the resource as defined in JSON. This is useful for debugging and
                                          // also to retrieve the resource by name.
};

struct RenderGraphSpecification
{
};

class RenderGraph final : private Uncopyable, private Unmovable
{
  public:
    RenderGraph(const std::string_view& debugName) : m_DebugName(debugName) {}
    ~RenderGraph() override = default;

    // void Render(const std::vector<RenderObject>& opaqueObjects, const std::vector<RenderObject>& transparentObjects);

    void AddNode(const RenderGraphNode& node)
    {
        if (m_Nodes.contains(node.Name)) return;

        m_Nodes[node.Name] = node;
    }

  private:
    std::unordered_map<std::string, RenderGraphNode> m_Nodes;
    std::string m_DebugName = s_DEFAULT_STRING;
};

}  // namespace Pathfinder

#endif