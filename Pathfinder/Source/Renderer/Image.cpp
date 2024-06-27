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

#if TODO
    if (AVX2Supported())
    {
        // Process 8 pixels at a time
        size_t i                     = 0;
        const size_t alignedDataSize = (dataSize & ~7);

        const __m256i alpha = _mm256_set1_epi32(255);    // Set all elements of alpha to 255
        for (size_t i = 0; i < alignedDataSize; i += 8)  // Process 8 elements at a time
        {
            // Load 24 bytes (8 RGB pixels) from rgb into two 256-bit registers
            const __m256i rgb1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rgb[3 * i]));
            const __m256i rgb2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rgb[3 * i + 24]));

            // Interleave the RGB values with alpha and store in rgba
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&rgba[4 * i]), _mm256_unpacklo_epi8(rgb1, alpha));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(&rgba[4 * i + 32]), _mm256_unpacklo_epi8(rgb2, alpha));
        }

        // Handle the remaining pixels if the total number of pixels is not a multiple of 8
        for (i = alignedDataSize; i < dataSize; ++i)
        {
            for (size_t k{}; k < 3; ++k)
            {
                rgba[4 * i + k] = rgb[3 * i + k];
            }
            rgba[4 * i + 3] = 255;
        }
    }
    else if (AVXSupported())
    {
        // Process 4 pixels at a time
        size_t i                     = 0;
        const size_t alignedDataSize = (dataSize & ~3);
        for (i = 0; i < alignedDataSize; i += 4)
        {
            // Load 4 RGB values into a 128-bit vector
            __m128i rgbVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rgb + 3 * i));

            // Set the alpha channel to 255 for all pixels
            __m128i alphaVec = _mm_set1_epi8(255);

            // Interleave the RGB values with the alpha channel
            __m128i rgbaVec = _mm_unpacklo_epi8(rgbVec, alphaVec);
            rgbaVec         = _mm_unpacklo_epi8(rgbaVec, _mm_setzero_si128());

            // Store the result back into the rgba buffer
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgba + 4 * i), rgbaVec);
        }

        // Handle the remaining pixels if the total number of pixels is not a multiple of 4
        for (i = alignedDataSize; i < dataSize; ++i)
        {
            for (size_t k{}; k < 3; ++k)
            {
                rgba[4 * i + k] = rgb[3 * i + k];
            }
            rgba[4 * i + 3] = 255;
        }
    }
    else
#endif
    {
        for (size_t i{}; i < dataSize; ++i)
        {
            for (size_t k{}; k < 3; ++k)
                rgba[4 * i + k] = rgb[3 * i + k];
            rgba[4 * i + 3] = 255;
        }
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
    LOG_TRACE("{}", __FUNCTION__);
}

}  // namespace Pathfinder