#include <PathfinderPCH.h>
#include "Buffer.h"

#include "RendererAPI.h"
#include <Platform/Vulkan/VulkanBuffer.h>

namespace Pathfinder
{

Shared<Buffer> Buffer::Create(const BufferSpecification& bufferSpec, const void* data, const size_t dataSize)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanBuffer>(bufferSpec, data, dataSize);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder