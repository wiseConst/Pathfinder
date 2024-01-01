#include "PathfinderPCH.h"
#include "CommandBuffer.h"
#include "RendererAPI.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Pathfinder
{
Shared<CommandBuffer> CommandBuffer::Create(ECommandBufferType type, ECommandBufferLevel level)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            return MakeShared<VulkanCommandBuffer>(type, level);
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}
}  // namespace Gauntlet