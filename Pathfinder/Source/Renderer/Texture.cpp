#include <PathfinderPCH.h>
#include "Texture.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"
#include <Renderer/Renderer.h>
#include <Renderer/DescriptorManager.h>

#include <compressonator.h>

namespace Pathfinder
{

Shared<Texture> Texture::Create(const TextureSpecification& textureSpec, const void* data, const size_t dataSize)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanTexture>(textureSpec, data, dataSize);
    }

    PFR_ASSERT(false, "Unknown Renderer API!");
    return nullptr;
}

void Texture::Invalidate(const void* data = nullptr, const size_t dataSize = 0)
{
    if (data && dataSize > 0) m_Specification.UsageFlags |= EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;

    ImageSpecification imageSpec = {.DebugName  = m_Specification.DebugName,
                                    .Width      = m_Specification.Width,
                                    .Height     = m_Specification.Height,
                                    .Format     = m_Specification.Format,
                                    .UsageFlags = m_Specification.UsageFlags,
                                    .Layers     = m_Specification.Layers};
    if (m_Specification.bGenerateMips)
    {
        imageSpec.Mips = ImageUtils::CalculateMipCount(m_Specification.Width, m_Specification.Height);
        imageSpec.UsageFlags |= EImageUsage::IMAGE_USAGE_TRANSFER_SRC_BIT;  // For downsampling the source.
    }

    m_Image = Image::Create(imageSpec);

    if (data && dataSize > 0)
    {
        m_Image->SetData(data, dataSize);
    }

    if (!(m_Specification.UsageFlags & EImageUsage::IMAGE_USAGE_STORAGE_BIT))
    {
        m_Image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
        if (m_Specification.bGenerateMips) GenerateMipMaps();
    }
}

void TextureCompressor::Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat, const void* rawImageData,
                                 const size_t rawImageSize, void** outImageData, size_t& outImageSize)
{
    PFR_ASSERT(rawImageData && rawImageSize > 0, "Invalid image data to compress!");

    CMP_Texture srcTexture = {};
    srcTexture.dwSize      = sizeof(srcTexture);
    srcTexture.dwWidth     = textureSpec.Width;
    srcTexture.dwHeight    = textureSpec.Height;

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

void TextureCompressor::SaveCompressed(const std::filesystem::path& savePath, const TextureSpecification& textureSpec,
                                       const void* imageData, const size_t imageSize)
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

void TextureCompressor::LoadCompressed(const std::filesystem::path& loadPath, TextureSpecification& outTextureSpec,
                                       std::vector<uint8_t>& outData)
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

void TextureManager::Init()
{
    {
        constexpr uint32_t whiteColor = 0xFFFFFFFF;
        s_WhiteTexture = Texture::Create({.DebugName = "Default_WhiteTexture", .Width = 1, .Height = 1}, &whiteColor, sizeof(whiteColor));
    }

    LOG_TRACE("{}", __FUNCTION__);
}

void TextureManager::Shutdown()
{
    s_WhiteTexture.reset();
    LOG_TRACE("{}", __FUNCTION__);
}

void TextureManager::LinkLoadedTexturesWithMeshes() {}

}  // namespace Pathfinder