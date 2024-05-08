#include "PathfinderPCH.h"
#include "Texture2D.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

#include "Core/Threading.h"
#include <compressonator.h>

namespace Pathfinder
{

Shared<Texture2D> Texture2D::Create(const TextureSpecification& textureSpec, const void* data, const size_t dataSize)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanTexture2D>(textureSpec, data, dataSize);
    }

    PFR_ASSERT(false, "Unknown Renderer API!");
    return nullptr;
}

void Texture2D::Invalidate(const void* data = nullptr, const size_t dataSize = 0)
{
    ImageSpecification imageSpec = {m_Specification.Width, m_Specification.Height};
    imageSpec.Format             = m_Specification.Format;
    imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;
    imageSpec.Layers             = m_Specification.Layers;
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

    m_Image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (m_Specification.bGenerateMips) GenerateMipMaps();
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
        {
            srcTexture.format  = CMP_FORMAT_R_8;
            srcTexture.dwPitch = srcTexture.dwWidth * 1;
            break;
        }
        case EImageFormat::FORMAT_RG8_UNORM:
        {
            srcTexture.format  = CMP_FORMAT_RG_8;
            srcTexture.dwPitch = srcTexture.dwWidth * 2;
            break;
        }
        case EImageFormat::FORMAT_RGBA8_UNORM:
        {
            srcTexture.format  = CMP_FORMAT_RGBA_8888;
            srcTexture.dwPitch = srcTexture.dwWidth * 4;
            break;
        }
        default:
        {
            PFR_ASSERT(false, "Other src image formats currently not implemented! TODO!");
            break;
        }
    }
    srcTexture.dwDataSize = rawImageSize;
    srcTexture.pData      = (CMP_BYTE*)rawImageData;

    CMP_Texture dstTexture = {};
    dstTexture.dwSize      = sizeof(dstTexture);
    dstTexture.dwWidth     = srcTexture.dwWidth;
    dstTexture.dwHeight    = srcTexture.dwHeight;
    dstTexture.dwPitch     = 0;
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

    const auto CMP_PrintInfoStr         = [](const char* InfoStr) { LOG_TAG_INFO(AMD_Compressonator, "%s", InfoStr); };
    CMP_CompressOptions compressOptions = {0};
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
        LOG_TAG_WARN(TextureCompressor, "Failed to save compressed texture! <%s>", savePath.string().data());
        return;
    }

    // Store texture specification and its data.
    out.write(reinterpret_cast<const char*>(&textureSpec), sizeof(textureSpec));
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
        LOG_TAG_WARN(TextureCompressor, "Failed to load compressed texture! <%s>", loadPath.string().data());
        return;
    }

    // Load texture specification.
    in.read(reinterpret_cast<char*>(&outTextureSpec), sizeof(outTextureSpec));

    // Load compressed image size.
    size_t dataSize = 0;
    in.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
    PFR_ASSERT(dataSize > 0, "Failed to load compressed image! DataSize is not > 0!");

    outData.resize(dataSize);
    in.read(reinterpret_cast<char*>(outData.data()), outData.size());

    in.close();
}

}  // namespace Pathfinder