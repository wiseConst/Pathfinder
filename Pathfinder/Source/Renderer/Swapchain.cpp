#include "PathfinderPCH.h"
#include "Swapchain.h"
#include "RendererAPI.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

namespace Pathfinder
{
Unique<Swapchain> Swapchain::Create(void* windowHandle)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanSwapchain>(windowHandle);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder