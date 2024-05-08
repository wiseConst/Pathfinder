#include "PathfinderPCH.h"
#include "CommandBuffer.h"
#include "RendererAPI.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Pathfinder
{
Shared<CommandBuffer> CommandBuffer::Create(const CommandBufferSpecification& commandBufferSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            return MakeShared<VulkanCommandBuffer>(commandBufferSpec);
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}
}  // namespace Pathfinder