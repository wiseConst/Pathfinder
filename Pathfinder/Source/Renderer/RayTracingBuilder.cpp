#include "PathfinderPCH.h"
#include "RayTracingBuilder.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanRayTracingBuilder.h"

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

            LOG_TAG_INFO(RAYTRACING_BUILDER, "VulkanRayTracingBuilder created!");
            break;
        }
        default:
        {
            PFR_ASSERT(false, "Unknown RendererAPI!");
        }
    }
}

void RayTracingBuilder::Shutdown()
{
    delete s_Instance;
    s_Instance = nullptr;
    LOG_TAG_INFO(RAYTRACING_BUILDER, "RayTracingBuilder destroyed!");
}

}  // namespace Pathfinder