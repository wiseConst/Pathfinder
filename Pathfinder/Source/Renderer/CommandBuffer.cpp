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

Shared<SyncPoint> SyncPoint::Create(void* timelineSemaphoreHandle, const uint64_t value, const RendererTypeFlags pipelineStages)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            return MakeShared<VulkanSyncPoint>(timelineSemaphoreHandle, value, pipelineStages);
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

Shared<QueryPool> QueryPool::Create(const uint32_t queryCount, const bool bIsPipelineStatistics)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            return MakeShared<VulkanQueryPool>(queryCount, bIsPipelineStatistics);
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder