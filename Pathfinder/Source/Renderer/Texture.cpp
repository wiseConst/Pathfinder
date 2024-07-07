#include <PathfinderPCH.h>
#include "Texture.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"
#include <Renderer/Renderer.h>
#include <Renderer/DescriptorManager.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <Renderer/Mesh/Submesh.h>
#include <Renderer/Material.h>

#include <compressonator.h>

namespace Pathfinder
{

namespace TextureUtils
{

void* LoadRawImage(const std::filesystem::path& imagePath, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels) noexcept
{
    PFR_ASSERT(!imagePath.empty() && x && y && nChannels, "Invalid data passed into LoadRawImage()!");

    const auto prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    void* imageData = stbi_load(imagePath.string().data(), x, y, nChannels, STBI_default);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);
    return imageData;
}

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels) noexcept
{
    PFR_ASSERT(data && x && y && nChannels, "Invalid data passed into LoadRawImageFromMemory()!");

    const auto prevFlipOnLoad = stbi__vertically_flip_on_load_global;
    stbi_set_flip_vertically_on_load(bFlipOnLoad);

    void* imageData = stbi_load_from_memory(data, dataSize, x, y, nChannels, STBI_default);
    PFR_ASSERT(imageData, "Failed to load image data!");

    stbi_set_flip_vertically_on_load(prevFlipOnLoad);
    return imageData;
}

void* ConvertRgbToRgba(const uint8_t* rgb, const uint32_t width, const uint32_t height) noexcept
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

void UnloadRawImage(void* data) noexcept
{
    stbi_image_free(data);
}

}  // namespace TextureUtils

Shared<Texture> Texture::Create(const TextureSpecification& textureSpec, const void* data, const size_t dataSize) noexcept
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanTexture>(textureSpec, data, dataSize);
    }

    PFR_ASSERT(false, "Unknown Renderer API!");
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

void TextureManager::Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat, const void* rawImageData,
                              const size_t rawImageSize, void** outImageData, size_t& outImageSize) noexcept
{
    PFR_ASSERT(rawImageData && rawImageSize > 0, "Invalid image data to compress!");

    CMP_Texture srcTexture = {};
    srcTexture.dwSize      = sizeof(srcTexture);
    srcTexture.dwWidth     = textureSpec.Dimensions.x;
    srcTexture.dwHeight    = textureSpec.Dimensions.y;

    switch (srcImageFormat)
    {
        case EImageFormat::FORMAT_R8_UNORM:
            srcTexture.format  = CMP_FORMAT_R_8;
            srcTexture.dwPitch = srcTexture.dwWidth * 1;
            break;
        case EImageFormat::FORMAT_RG8_UNORM:
            srcTexture.format  = CMP_FORMAT_RG_8;
            srcTexture.dwPitch = srcTexture.dwWidth * 2;
            break;
        case EImageFormat::FORMAT_RGBA8_UNORM:
            srcTexture.format  = CMP_FORMAT_RGBA_8888;
            srcTexture.dwPitch = srcTexture.dwWidth * 4;
            break;
        default: PFR_ASSERT(false, "Other src image formats currently not implemented! TODO!"); break;
    }
    srcTexture.dwDataSize = rawImageSize;
    srcTexture.pData      = (CMP_BYTE*)rawImageData;

    CMP_Texture dstTexture = {.dwWidth = srcTexture.dwWidth, .dwHeight = srcTexture.dwHeight, .dwPitch = 0};
    dstTexture.dwSize      = sizeof(dstTexture);

    switch (textureSpec.Format)
    {
        case EImageFormat::FORMAT_BC1_RGB_UNORM:
        case EImageFormat::FORMAT_BC1_RGB_SRGB:
        case EImageFormat::FORMAT_BC1_RGBA_UNORM:
        case EImageFormat::FORMAT_BC1_RGBA_SRGB: dstTexture.format = CMP_FORMAT_BC1; break;

        case EImageFormat::FORMAT_BC2_UNORM:
        case EImageFormat::FORMAT_BC2_SRGB: dstTexture.format = CMP_FORMAT_BC2; break;

        case EImageFormat::FORMAT_BC3_UNORM:
        case EImageFormat::FORMAT_BC3_SRGB: dstTexture.format = CMP_FORMAT_BC3; break;

        case EImageFormat::FORMAT_BC4_UNORM: dstTexture.format = CMP_FORMAT_BC4; break;
        case EImageFormat::FORMAT_BC4_SNORM: dstTexture.format = CMP_FORMAT_BC4_S; break;

        case EImageFormat::FORMAT_BC5_UNORM: dstTexture.format = CMP_FORMAT_BC5; break;
        case EImageFormat::FORMAT_BC5_SNORM: dstTexture.format = CMP_FORMAT_BC5_S; break;

        case EImageFormat::FORMAT_BC6H_UFLOAT: dstTexture.format = CMP_FORMAT_BC6H; break;
        case EImageFormat::FORMAT_BC6H_SFLOAT: dstTexture.format = CMP_FORMAT_BC6H_SF; break;

        case EImageFormat::FORMAT_BC7_UNORM:
        case EImageFormat::FORMAT_BC7_SRGB: dstTexture.format = CMP_FORMAT_BC7; break;
    }

    // NOTE: Well, it seems so AMD Compressonator isn't thread safe.
    std::scoped_lock lock(s_CompressorMutex);

    dstTexture.dwDataSize = CMP_CalculateBufferSize(&dstTexture);
    dstTexture.pData      = (CMP_BYTE*)malloc(dstTexture.dwDataSize);

#if LOG_TEXTURE_COMPRESSION_INFO
    LOG_TAG_INFO(AMD_COMPRESSONATOR, "Succeed to move from: %llu bytes, to %llu bytes, compression ratio: %f %.", rawImageSize,
                 dstTexture.dwDataSize, rawImageSize / (float)dstTexture.dwDataSize);
#endif

    const auto CMP_PrintInfoStr         = [](const char* InfoStr) { LOG_INFO("AMD_Compressonator: {}", InfoStr); };
    CMP_CompressOptions compressOptions = {};
    compressOptions.dwSize              = sizeof(compressOptions);
    compressOptions.m_PrintInfoStr      = CMP_PrintInfoStr;
    compressOptions.fquality            = 0.05f;  // 0.88f;
    compressOptions.bUseGPUDecompress   = true;
    compressOptions.bUseCGCompress      = true;
    compressOptions.nEncodeWith         = CMP_GPU_VLK;

#if LOG_TEXTURE_COMPRESSION_INFO
    const CMP_Feedback_Proc compressionCallback = [](CMP_FLOAT fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2) -> bool
    {
        LOG_TAG_TRACE(AMD_COMPRESSONATOR, "Conversion texture progress: (%0.3f)", fProgress);
        return 0;
    };
#endif

    const auto compressionStatus = CMP_ConvertTexture(&srcTexture, &dstTexture, &compressOptions,
#if LOG_TEXTURE_COMPRESSION_INFO
                                                      compressionCallback
#else
                                                      nullptr
#endif
    );
    PFR_ASSERT(compressionStatus == CMP_OK, "Failed to convert texture using AMD Compressonator!");

    *outImageData = dstTexture.pData;
    outImageSize  = dstTexture.dwDataSize;
}

void TextureManager::SaveCompressed(const std::filesystem::path& savePath, const TextureSpecification& textureSpec, const void* imageData,
                                    const size_t imageSize) noexcept
{
    PFR_ASSERT(!savePath.empty(), "Invalid save path for compressed texture!");
    PFR_ASSERT(imageData && imageSize > 0, "Invalid image data to save!");

    std::ofstream out(savePath, std::ios::out | std::ios::binary);
    if (!out.is_open())
    {
        LOG_WARN("TextureCompressor: Failed to save compressed texture! <{}>", savePath.string());
        return;
    }

    // Store texture specification and its data.

    const auto debugNameLength = textureSpec.DebugName.size();
    out.write(reinterpret_cast<const char*>(&debugNameLength), sizeof(debugNameLength));
    out.write(textureSpec.DebugName.data(), debugNameLength);
    out.write(reinterpret_cast<const char*>(&textureSpec) + sizeof(std::string), sizeof(textureSpec) - sizeof(std::string));
    out.write(reinterpret_cast<const char*>(&imageSize), sizeof(imageSize));
    out.write(reinterpret_cast<const char*>(imageData), imageSize);

    out.close();
}

void TextureManager::LoadCompressed(const std::filesystem::path& loadPath, TextureSpecification& outTextureSpec,
                                    std::vector<uint8_t>& outData) noexcept
{
    PFR_ASSERT(!loadPath.empty(), "Invalid load path for compressed texture!");

    std::ifstream in(loadPath, std::ios::in | std::ios::binary);
    if (!in.is_open())
    {
        LOG_WARN("TextureCompressor: Failed to load compressed texture! <{}>", loadPath.string());
        return;
    }

    // Load texture specification.
    size_t debugNameLength = 0;
    in.read(reinterpret_cast<char*>(&debugNameLength), sizeof(debugNameLength));

    outTextureSpec.DebugName.resize(debugNameLength);
    in.read(outTextureSpec.DebugName.data(), debugNameLength);

    in.read(reinterpret_cast<char*>(&outTextureSpec) + sizeof(std::string), sizeof(outTextureSpec) - sizeof(std::string));

    // Load compressed image size.
    size_t dataSize = 0;
    in.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
    PFR_ASSERT(dataSize > 0, "Failed to load compressed image! DataSize is not > 0!");

    outData.resize(dataSize);
    in.read(reinterpret_cast<char*>(outData.data()), outData.size());

    in.close();
}

void TextureManager::Init() noexcept
{
    SamplerStorage::Init();

    {
        constexpr uint32_t whiteColor = 0xFFFFFFFF;
        s_WhiteTexture =
            Texture::Create({.DebugName  = "Default_WhiteTexture",
                             .Dimensions = glm::uvec3{1, 1, 1},
                             .UsageFlags = EImageUsage::IMAGE_USAGE_COLOR_ATTACHMENT_BIT | EImageUsage::IMAGE_USAGE_SAMPLED_BIT},
                            &whiteColor, sizeof(whiteColor));
    }

    LOG_TRACE("{}", __FUNCTION__);
}

void TextureManager::Shutdown() noexcept
{
    s_WhiteTexture.reset();
    LOG_TRACE("{}", __FUNCTION__);

    SamplerStorage::Shutdown();
}

void TextureManager::LinkLoadedTexturesWithMeshes() noexcept
{
    // for (auto it = s_TexturesInProcess.begin(); it != s_TexturesInProcess.end();)
    //{
    //     auto& loadedEntry = *it;

    //    // Don't block main thread.
    //    if (loadedEntry.TextureFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    //    {
    //        ++it;
    //        continue;
    //    }

    //    const auto texture = loadedEntry.TextureFuture.get();
    //    LOG_INFO("{}: {}", __FUNCTION__, texture->GetSpecification().DebugName);

    //    if (auto submesh = loadedEntry.SubmeshPtr.lock())
    //    {
    //        auto& material = submesh->GetMaterial();
    //        switch (loadedEntry.TextureLoadType)
    //        {
    //            case ETextureLoadType::TEXTURE_LOAD_TYPE_ALBEDO: material->SetAlbedo(texture); break;
    //            case ETextureLoadType::TEXTURE_LOAD_TYPE_NORMAL: material->SetNormalMap(texture); break;
    //            case ETextureLoadType::TEXTURE_LOAD_TYPE_METALLIC_ROUGHNESS: material->SetMetallicRoughnessMap(texture); break;
    //            case ETextureLoadType::TEXTURE_LOAD_TYPE_OCCLUSION: material->SetOcclusionMap(texture); break;
    //            case ETextureLoadType::TEXTURE_LOAD_TYPE_EMISSIVE: material->SetEmissiveMap(texture); break;
    //            default: PFR_ASSERT(false, "Unknown ETextureLoadType!");
    //        }

    //        material->Update();
    //    }

    //    it = s_TexturesInProcess.erase(it);
    //}

    // s_TexturesInProcess.clear();
}

}  // namespace Pathfinder