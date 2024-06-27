#include <PathfinderPCH.h>
#include "DescriptorManager.h"

#include "RendererAPI.h"
#include <Platform/Vulkan/VulkanDescriptorManager.h>

namespace Pathfinder
{

Shared<DescriptorManager> DescriptorManager::Create()
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanDescriptorManager>();
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder