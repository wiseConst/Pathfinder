#include "PathfinderPCH.h"
#include "UILayer.h"

#include "Renderer/RendererAPI.h"
#include "Platform/Vulkan/VulkanUILayer.h"

namespace Pathfinder
{

Unique<UILayer> UILayer::Create()
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeUnique<VulkanUILayer>();
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder