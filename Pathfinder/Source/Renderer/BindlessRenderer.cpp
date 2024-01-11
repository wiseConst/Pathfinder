#include "PathfinderPCH.h"
#include "BindlessRenderer.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanBindlessRenderer.h"

namespace Pathfinder
{

Shared<BindlessRenderer> BindlessRenderer::Create()
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanBindlessRenderer>();
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder