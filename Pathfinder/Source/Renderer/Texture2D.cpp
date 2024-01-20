#include "PathfinderPCH.h"
#include "Texture2D.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

namespace Pathfinder
{
Shared<Texture2D> Texture2D::Create(const TextureSpecification& textureSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanTexture2D>(textureSpec);
    }

    PFR_ASSERT(false, "Unknown Renderer API!");
    return nullptr;
}

}  // namespace Pathfinder