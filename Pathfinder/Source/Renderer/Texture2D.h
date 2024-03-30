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
    void* Data            = nullptr;
    size_t DataSize       = 0;
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

    NODISCARD static Shared<Texture2D> Create(const TextureSpecification& textureSpec);

  protected:
    Shared<Image> m_Image                = nullptr;
    TextureSpecification m_Specification = {};
    uint32_t m_Index                     = UINT32_MAX;  // bindless array purposes

    Texture2D(const TextureSpecification& textureSpec) : m_Specification(textureSpec) {}
    Texture2D() = default;

    virtual void Destroy() = 0;
    virtual void Invalidate();
};

class TextureCompressor final : private Uncopyable, private Unmovable
{
  public:
    ~TextureCompressor() override = default;

    // NOTE:
    // Firstly textureSpec should contatin raw image data.
    // Compresses data from srcImageFormat into textureSpec.Format
    // Stores data into textureSpec.Data && DataSize, so you have to manually free it after Texture2D::Create().
    static void Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat);

    static void SaveCompressed(const TextureSpecification& textureSpec, const std::filesystem::path& savePath);

    // NOTE:
    // Loads textureSpec.Data with its DataSize, so you'll have to manually free() it after Texture2D::Create().
    static void LoadCompressed(TextureSpecification& textureSpec, const std::filesystem::path& loadPath);

  private:
    TextureCompressor() = default;
};

}  // namespace Pathfinder

#endif