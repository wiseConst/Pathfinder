#include "PathfinderPCH.h"
#include "Framebuffer.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanFramebuffer.h"

namespace Pathfinder
{

Shared<Framebuffer> Framebuffer::Create(const FramebufferSpecification& framebufferSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanFramebuffer>(framebufferSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder