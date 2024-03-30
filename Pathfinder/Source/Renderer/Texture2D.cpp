#include "PathfinderPCH.h"
#include "Texture2D.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanTexture2D.h"

#include "Core/Threading.h"
#include <compressonator.h>

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

void Texture2D::Invalidate()
{
    if (m_Image)
    {
        PFR_ASSERT(false, "No texture invalidation implemented && tested!");
        return;
    }

    ImageSpecification imageSpec = {m_Specification.Width, m_Specification.Height};
    imageSpec.Format             = m_Specification.Format;
    imageSpec.UsageFlags         = EImageUsage::IMAGE_USAGE_SAMPLED_BIT | EImageUsage::IMAGE_USAGE_TRANSFER_DST_BIT;
    imageSpec.Layers             = m_Specification.Layers;

    m_Image = Image::Create(imageSpec);

    m_Image->SetData(m_Specification.Data, m_Specification.DataSize);
    m_Image->SetLayout(EImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void TextureCompressor::Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat)
{
    PFR_ASSERT(textureSpec.Data && textureSpec.DataSize > 0, "Invalid image data to compress!");

    CMP_Texture srcTexture = {};
    srcTexture.dwSize      = sizeof(srcTexture);
    srcTexture.dwWidth     = textureSpec.Width;
    srcTexture.dwHeight    = textureSpec.Height;

    switch (srcImageFormat)
    {
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
    srcTexture.dwDataSize = textureSpec.DataSize;
    srcTexture.pData      = (CMP_BYTE*)textureSpec.Data;

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

    const auto CMP_PrintInfoStr         = [](const char* InfoStr) { LOG_TAG_INFO(AMD_Compressonator, "%s", InfoStr); };
    CMP_CompressOptions compressOptions = {0};
    compressOptions.dwSize              = sizeof(compressOptions);
    compressOptions.m_PrintInfoStr      = CMP_PrintInfoStr;
    compressOptions.fquality            = 0.88f;
    compressOptions.dwnumThreads        = JobSystem::GetNumThreads();

    const auto compressionStatus = CMP_ConvertTexture(&srcTexture, &dstTexture, &compressOptions, nullptr);
    PFR_ASSERT(compressionStatus == CMP_OK, "Failed to convert texture using AMD Compressonator!");

    textureSpec.Data     = dstTexture.pData;
    textureSpec.DataSize = dstTexture.dwDataSize;
}

void TextureCompressor::SaveCompressed(const TextureSpecification& textureSpec, const std::filesystem::path& savePath)
{
    PFR_ASSERT(textureSpec.Data && textureSpec.DataSize > 0, "Invalid image data to save!");
    PFR_ASSERT(!savePath.empty(), "Invalid save path for compressed texture!");

    std::ofstream out(savePath, std::ios::out | std::ios::binary);
    if (!out.is_open())
    {
        LOG_TAG_WARN(TextureCompressor, "Failed to save compressed texture! <%s>", savePath.string().data());
        return;
    }

    // Store texture header("specification"), then its data.
    out.write(reinterpret_cast<const char*>(&textureSpec), sizeof(textureSpec));
    out.write(reinterpret_cast<const char*>(textureSpec.Data), textureSpec.DataSize);

    out.close();
}

void TextureCompressor::LoadCompressed(TextureSpecification& textureSpec, const std::filesystem::path& loadPath)
{
    PFR_ASSERT(!loadPath.empty(), "Invalid load path for compressed texture!");

    std::ifstream in(loadPath, std::ios::in | std::ios::binary);
    if (!in.is_open())
    {
        LOG_TAG_WARN(TextureCompressor, "Failed to load compressed texture! <%s>", loadPath.string().data());
        return;
    }

    // Load texture header("specification").
    in.read(reinterpret_cast<char*>(&textureSpec), sizeof(textureSpec));

    // Allocate space for compressed data. (should be free() after Texture2D::Create()).
    textureSpec.Data = malloc(textureSpec.DataSize);
    in.read(reinterpret_cast<char*>(textureSpec.Data), textureSpec.DataSize);

    in.close();
}

}  // namespace Pathfinder