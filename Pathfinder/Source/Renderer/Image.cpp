#include "PathfinderPCH.h"
#include "Image.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanImage.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Pathfinder
{
namespace ImageUtils
{

void* LoadRawImage(std::string_view path, int32_t* x, int32_t* y, int32_t* nChannels)
{
    PFR_ASSERT(path.data() && x && y && nChannels, "Invalid data passed into LoadRawImage()!");

    int32_t desiredChannels = 4;
    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN) desiredChannels = 4;

    void* imageData = stbi_load(path.data(), x, y, nChannels, desiredChannels);
    PFR_ASSERT(imageData, "Failed to load image data!");

    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN && *nChannels != 4) *nChannels = desiredChannels;
    return imageData;
}

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, int32_t* x, int32_t* y, int32_t* nChannels)
{
    PFR_ASSERT(data && x && y && nChannels, "Invalid data passed into LoadRawImageFromMemory()!");

    int32_t desiredChannels = 4;
    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN) desiredChannels = 4;

    void* imageData = stbi_load_from_memory(data, dataSize, x, y, nChannels, desiredChannels);
    PFR_ASSERT(imageData, "Failed to load image data!");

    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN && *nChannels != 4) *nChannels = desiredChannels;
    return imageData;
}

void UnloadRawImage(void* data)
{
    stbi_image_free(data);
}

}  // namespace ImageUtils

Shared<Image> Image::Create(const ImageSpecification& imageSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanImage>(imageSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

void SamplerStorage::Init()
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            s_Instance = new VulkanSamplerStorage();
            break;
        }
        default: PFR_ASSERT(false, "Unknown RendererAPI!");
    }
}

}  // namespace Pathfinder