#include "PathfinderPCH.h"
#include "Buffer.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanBuffer.h"

namespace Pathfinder
{

static Unique<Buffer> Create(const BufferSpecification& bufferSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanBuffer>(bufferSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder