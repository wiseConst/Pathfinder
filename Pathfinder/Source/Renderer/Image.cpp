#include "PathfinderPCH.h"
#include "Image.h"

#include "Renderer/Renderer.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanImage.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Pathfinder
{
namespace ImageUtils
{

void* LoadRawImage(const std::filesystem::path& imagePath, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels)
{
    PFR_ASSERT(!imagePath.empty() && x && y && nChannels, "Invalid data passed into LoadRawImage()!");

    const int32_t prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    int32_t desiredChannels = 4;
    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN) desiredChannels = 4;

    void* imageData = stbi_load(imagePath.string().data(), x, y, nChannels, desiredChannels);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);
    return imageData;
}

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels)
{
    PFR_ASSERT(data && x && y && nChannels, "Invalid data passed into LoadRawImageFromMemory()!");

    const int32_t prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    int32_t desiredChannels = 4;
    if (RendererAPI::Get() == ERendererAPI::RENDERER_API_VULKAN) desiredChannels = 4;

    void* imageData = stbi_load_from_memory(data, dataSize, x, y, nChannels, desiredChannels);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);

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