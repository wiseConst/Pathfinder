#pragma once

#include <Core/Core.h>
#include <Core/ThreadPool.h>
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

    NODISCARD FORCEINLINE const auto& GetUUID() const { return m_UUID; }
    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE const auto GetBindlessIndex() const
    {
        PFR_ASSERT(m_BindlessIndex.has_value(), "Texture doesn't have bindless index!");
        return m_BindlessIndex.value();
    }
    NODISCARD FORCEINLINE const auto& GetImage() const { return m_Image; }

    NODISCARD static Shared<Texture> Create(const TextureSpecification& textureSpec, const void* data = nullptr, const size_t dataSize = 0);

    virtual void Resize(const uint32_t width, const uint32_t height) = 0;
    virtual void SetDebugName(const std::string& name)               = 0;

  protected:
    Shared<Image> m_Image                = nullptr;
    TextureSpecification m_Specification = {};
    Optional<uint32_t> m_BindlessIndex   = std::nullopt;
    UUID m_UUID                          = {};  // NOTE: Used only for imgui purposes.

    Texture(const TextureSpecification& textureSpec) : m_Specification(textureSpec) {}
    Texture() = delete;

    virtual void Destroy() = 0;
    virtual void Invalidate(const void* data, const size_t dataSize);
    virtual void GenerateMipMaps() = 0;
};

class TextureCompressor final
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
    static inline std::mutex s_CompressorMutex;

    TextureCompressor()  = delete;
    ~TextureCompressor() = default;
};

enum class ETextureLoadType : uint8_t
{
    TEXTURE_LOAD_TYPE_ALBEDO,
    TEXTURE_LOAD_TYPE_NORMAL,
    TEXTURE_LOAD_TYPE_METALLIC_ROUGHNESS,
    TEXTURE_LOAD_TYPE_EMISSIVE,
    TEXTURE_LOAD_TYPE_OCCLUSION
};

// TODO: Parallel texture loading/compressing.
class Submesh;
class TextureManager final
{
  public:
    static void Init();
    static void Shutdown();

    NODISCARD FORCEINLINE static const auto& GetWhiteTexture() { return s_WhiteTexture; }

    template <typename Func, typename... Args>
    static Shared<Texture>& LoadTextureLazy(const ETextureLoadType textureLoadType, Weak<Submesh> submesh, const std::string& baseMeshDir,
                                            const std::string& textureName, Func&& func, Args&&... args)
    {
        //const std::string wholeTextureName = baseMeshDir + textureName;

        ////    if (loadedTextures.contains(textureName)) return loadedTextures[textureName];

        ///*baseMeshDir + textureName*/
        //s_TexturesInProcess.emplace_back(ThreadPool::Submit(std::forward<Func>(func), std::forward<Args>(args)...), submesh,
        //                                 textureLoadType);

        return s_WhiteTexture;
    }

    static void LinkLoadedTexturesWithMeshes();

  private:
    static inline Shared<Texture> s_WhiteTexture = nullptr;
    //    static inline Pool<Shared<Texture>> s_TexturePool;

    //static inline UnorderedMap<std::string, uint32_t> s_TexturesInProcessNameToIndex;

    //struct LoadedTextureEntry
    //{
    //    std::shared_future<Shared<Texture>> TextureFuture;
    //    Weak<Submesh> SubmeshPtr;
    //    ETextureLoadType TextureLoadType;
    //};
    //static inline std::vector<LoadedTextureEntry> s_TexturesInProcess;

    TextureManager()  = delete;
    ~TextureManager() = default;
};

}  // namespace Pathfinder
