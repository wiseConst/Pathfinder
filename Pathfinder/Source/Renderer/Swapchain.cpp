#include "PathfinderPCH.h"
#include "Swapchain.h"
#include "Core/Application.h"

#include "Platform/Vulkan/VulkanSwapchain.h"

namespace Pathfinder
{
Unique<Swapchain> Swapchain::Create()
{
    switch (Application::Get().GetSpecification().RendererAPI)
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanSwapchain>();
    }

    return nullptr;
}

}  // namespace Pathfinder