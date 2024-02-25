#include "PathfinderPCH.h"
#include "CommandBuffer.h"
#include "RendererAPI.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Pathfinder
{
Shared<CommandBuffer> CommandBuffer::Create(ECommandBufferType type, bool bSignaledFence, ECommandBufferLevel level)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            return MakeShared<VulkanCommandBuffer>(type, bSignaledFence, level);
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}
}  // namespace Pathfinder