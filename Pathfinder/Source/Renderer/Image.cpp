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

    const auto prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    void* imageData = stbi_load(imagePath.string().data(), x, y, nChannels, STBI_default);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);
    return imageData;
}

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels)
{
    PFR_ASSERT(data && x && y && nChannels, "Invalid data passed into LoadRawImageFromMemory()!");

    const auto prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    void* imageData = stbi_load_from_memory(data, dataSize, x, y, nChannels, STBI_default);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);
    return imageData;
}

void* ConvertRgbToRgba(const uint8_t* rgb, const uint32_t width, const uint32_t height)
{
    PFR_ASSERT(rgb && width > 0 && height > 0, "Invalid data passed into ConvertRgbToRgba()!");

    const size_t dataSize = static_cast<size_t>(width) * static_cast<size_t>(height);
    uint8_t* rgba         = new uint8_t[dataSize * 4];
    PFR_ASSERT(rgba, "Failed to allocate rgba buffer for ConvertRgbToRgba().");

    // TODO: AVX256ify it.
    for (size_t i{}; i < dataSize; ++i)
    {
        for (size_t k{}; k < 3; ++k)
            rgba[4 * i + k] = rgb[3 * i + k];
        rgba[4 * i + 3] = 255;
    }

    return rgba;
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