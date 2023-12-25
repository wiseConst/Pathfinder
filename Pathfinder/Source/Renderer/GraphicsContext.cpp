#include "PathfinderPCH.h"
#include "GraphicsContext.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Pathfinder
{

Unique<GraphicsContext> GraphicsContext::Create(const ERendererAPI rendererApi)
{
    PFR_ASSERT(!s_Instance, "You can't have more than 1 graphics context per application instance!");

    switch (rendererApi)
    {
        case ERendererAPI::RENDERER_API_NONE: break;
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanContext>();
    }

    return nullptr;
}

}  // namespace Pathfinder
