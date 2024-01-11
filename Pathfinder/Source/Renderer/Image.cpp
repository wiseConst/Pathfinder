#include "PathfinderPCH.h"
#include "Image.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanImage.h"

namespace Pathfinder
{

Shared<Image> Image::Create(const ImageSpecification& imageSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanImage>(imageSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

}  // namespace Pathfinder