#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"
#include "Image.h"

namespace Pathfinder
{

struct TextureSpecification
{
    uint32_t Width        = 1;
    uint32_t Height       = 1;
    bool bGenerateMips    = false;
    bool bFlipOnLoad      = false;
    ESamplerWrap Wrap     = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    EImageFormat Format   = EImageFormat::FORMAT_RGBA8_UNORM;
    uint32_t Layers       = 1;
    bool bBindlessUsage   = false;
};

class Texture2D : private Uncopyable, private Unmovable
{
  public:
    virtual ~Texture2D() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const { return m_Index; }
    NODISCARD FORCEINLINE const auto& GetImage() const { return m_Image; }

    NODISCARD static Shared<Texture2D> Create(const TextureSpecification& textureSpec, const void* data = nullptr,
                                              const size_t dataSize = 0);

  protected:
    Shared<Image> m_Image                = nullptr;
    TextureSpecification m_Specification = {};
    uint32_t m_Index                     = UINT32_MAX;  // bindless array purposes

    Texture2D(const TextureSpecification& textureSpec) : m_Specification(textureSpec) {}
    Texture2D() = delete;

    virtual void Destroy() = 0;
    virtual void Invalidate(const void* data, const size_t dataSize);
    virtual void GenerateMipMaps() = 0;
};

class TextureCompressor final : private Uncopyable, private Unmovable
{
  public:
    ~TextureCompressor() override = default;

    // NOTE:
    // Compresses data from srcImageFormat into textureSpec.Format
    // outImageData will be fullfiled, so you have to free() it manually.
    static void Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat, const void* rawImageData,
                         const size_t rawImageSize, void** outImageData, size_t& outImageSize);

    static void SaveCompressed(const std::filesystem::path& savePath, const TextureSpecification& textureSpec, const void* imageData,
                               const size_t imageSize);

    static void LoadCompressed(const std::filesystem::path& loadPath, TextureSpecification& outTextureSpec, std::vector<uint8_t>& outData);

  private:
    TextureCompressor() = default;
};

}  // namespace Pathfinder

#endif