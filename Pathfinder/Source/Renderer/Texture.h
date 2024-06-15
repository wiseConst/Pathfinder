#pragma once

#include <Core/Core.h>
#include "RendererCoreDefines.h"
#include "Image.h"

namespace Pathfinder
{

struct TextureSpecification
{
    std::string DebugName      = s_DEFAULT_STRING;
    uint32_t Width             = 1;
    uint32_t Height            = 1;
    bool bGenerateMips         = false;
    ESamplerWrap Wrap          = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter      = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    EImageFormat Format        = EImageFormat::FORMAT_RGBA8_UNORM;
    ImageUsageFlags UsageFlags = EImageUsage::IMAGE_USAGE_SAMPLED_BIT;
    uint32_t Layers            = 1;
};

class Texture : private Uncopyable, private Unmovable
{
  public:
    virtual ~Texture() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE const auto GetBindlessIndex() const
    {
        PFR_ASSERT(m_BindlessIndex.has_value(), "Texture doesn't have bindless index!");
        return m_BindlessIndex.value();
    }
    NODISCARD FORCEINLINE const auto& GetImage() const { return m_Image; }

    NODISCARD static Shared<Texture> Create(const TextureSpecification& textureSpec, const void* data = nullptr,
                                              const size_t dataSize = 0);

    virtual void Resize(const uint32_t width, const uint32_t height) = 0;

  protected:
    Shared<Image> m_Image                   = nullptr;
    TextureSpecification m_Specification    = {};
    std::optional<uint32_t> m_BindlessIndex = std::nullopt;

    Texture(const TextureSpecification& textureSpec) : m_Specification(textureSpec) {}
    Texture() = delete;

    virtual void Destroy() = 0;
    virtual void Invalidate(const void* data, const size_t dataSize);
    virtual void GenerateMipMaps() = 0;
};

class TextureCompressor final : private Uncopyable, private Unmovable
{
  public:
    // NOTE:
    // Compresses data from srcImageFormat into textureSpec.Format
    // outImageData will be fullfiled, so you have to free() it manually.
    static void Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat, const void* rawImageData,
                         const size_t rawImageSize, void** outImageData, size_t& outImageSize);

    static void SaveCompressed(const std::filesystem::path& savePath, const TextureSpecification& textureSpec, const void* imageData,
                               const size_t imageSize);

    static void LoadCompressed(const std::filesystem::path& loadPath, TextureSpecification& outTextureSpec, std::vector<uint8_t>& outData);

  private:
    TextureCompressor()  = default;
    ~TextureCompressor() = default;
};

}  // namespace Pathfinder
