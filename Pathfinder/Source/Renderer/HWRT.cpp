#include <PathfinderPCH.h>
#include "HWRT.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanHWRT.h"

namespace Pathfinder
{

void RayTracingBuilder::Init()
{
    PFR_ASSERT(!s_Instance, "You can't have more than 1 graphics context per application instance!");

    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            s_Instance = new VulkanRayTracingBuilder();
            break;
        }
        default:
        {
            PFR_ASSERT(false, "Unknown RendererAPI!");
        }
    }
    LOG_TRACE("{}", __FUNCTION__);
}

void RayTracingBuilder::Shutdown()
{
    delete s_Instance;
    s_Instance = nullptr;
    LOG_TRACE("{}", __FUNCTION__);
}

}  // namespace Pathfinder