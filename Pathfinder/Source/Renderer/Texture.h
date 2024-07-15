#pragma once

#include <Core/Core.h>
#include <Core/ThreadPool.h>
#include "RendererCoreDefines.h"

namespace Pathfinder
{

// NOTE: Bindless by default, once and forever.
struct TextureSpecification
{
    std::string DebugName = s_DEFAULT_STRING;
    glm::uvec3 Dimensions{0};
    bool bGenerateMips         = false;
    bool bExposeMips           = false;  // Provide exclusive access to each mip for shader through bindless index.
    ESamplerWrap WrapS         = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerWrap WrapT         = WrapS;
    ESamplerFilter MinFilter   = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    ESamplerFilter MagFilter   = MinFilter;
    EImageFormat Format        = EImageFormat::FORMAT_RGBA8_UNORM;
    ImageUsageFlags UsageFlags = EImageUsage::IMAGE_USAGE_SAMPLED_BIT;
    uint32_t Layers            = 1;
};

class Texture : private Uncopyable, private Unmovable
{
  public:
    virtual ~Texture() = default;

    NODISCARD FORCEINLINE virtual void* Get() const noexcept     = 0;
    NODISCARD FORCEINLINE virtual void* GetView() const noexcept = 0;

    NODISCARD FORCEINLINE const auto& GetUUID() const noexcept { return m_UUID; }
    NODISCARD FORCEINLINE const auto& GetSpecification() const noexcept { return m_Specification; }
    NODISCARD FORCEINLINE const auto GetTextureBindlessIndex() const noexcept
    {
        PFR_ASSERT(m_TextureBindlessIndex.has_value(), "TextureBindlessIndex is invalid!");
        return m_TextureBindlessIndex.value();
    }

    NODISCARD FORCEINLINE const auto GetStorageTextureBindlessIndex() const noexcept
    {
        PFR_ASSERT(m_StorageTextureBindlessIndex.has_value(), "StorageTextureBindlessIndex is invalid!");
        return m_StorageTextureBindlessIndex.value();
    }

    NODISCARD FORCEINLINE const auto GetLayout() const noexcept { return m_Layout; }
    NODISCARD FORCEINLINE const auto GetMipCount() const noexcept { return m_Mips; }

    NODISCARD static Shared<Texture> Create(const TextureSpecification& textureSpec, const void* data = nullptr,
                                            const size_t dataSize = 0) noexcept;

    virtual void SetLayout(const EImageLayout newLayout, const bool bImmediate = false) noexcept               = 0;
    virtual void ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const noexcept = 0;
    virtual void Resize(const glm::uvec3& dimensions) noexcept                                                 = 0;
    virtual void SetDebugName(const std::string& name) noexcept                                                = 0;
    virtual void SetData(const void* data, size_t dataSize) noexcept                                           = 0;

  protected:
    TextureSpecification m_Specification             = {};
    Optional<uint32_t> m_TextureBindlessIndex        = std::nullopt;
    Optional<uint32_t> m_StorageTextureBindlessIndex = std::nullopt;
    UUID m_UUID                                      = {};  // NOTE: Used only for imgui purposes.

    // TODO:
    std::vector<uint32_t> m_TextureMipChainBindlessIndices;
    std::vector<uint32_t> m_StorageTextureMipChainBindlessIndices;

    EImageLayout m_Layout = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
    uint8_t m_Mips        = 1;

    Texture(const TextureSpecification& textureSpec) noexcept : m_Specification(textureSpec) {}
    Texture() = delete;

    virtual void Destroy() noexcept                                           = 0;
    virtual void Invalidate(const void* data, const size_t dataSize) noexcept = 0;
    virtual void GenerateMipMaps() noexcept                                   = 0;
};

namespace TextureUtils
{

FORCEINLINE NODISCARD static bool IsDepthFormat(const EImageFormat imageFormat) noexcept
{
    switch (imageFormat)
    {
        case EImageFormat::FORMAT_D16_UNORM:
        case EImageFormat::FORMAT_D32F:
        case EImageFormat::FORMAT_S8_UINT:
        case EImageFormat::FORMAT_D24_UNORM_S8_UINT:
        case EImageFormat::FORMAT_D16_UNORM_S8_UINT:
        case EImageFormat::FORMAT_D32_SFLOAT_S8_UINT: return true;
        default: return false;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return false;
}

FORCEINLINE NODISCARD static bool IsStencilFormat(const EImageFormat imageFormat) noexcept
{
    switch (imageFormat)
    {
        case EImageFormat::FORMAT_S8_UINT:
        case EImageFormat::FORMAT_D24_UNORM_S8_UINT:
        case EImageFormat::FORMAT_D16_UNORM_S8_UINT:
        case EImageFormat::FORMAT_D32_SFLOAT_S8_UINT: return true;
        default: return false;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return false;
}

void* LoadRawImage(const std::filesystem::path& imagePath, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels) noexcept;

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels) noexcept;

void* ConvertRgbToRgba(const uint8_t* rgb, const uint32_t width, const uint32_t height) noexcept;

void UnloadRawImage(void* data) noexcept;

FORCEINLINE NODISCARD static uint32_t CalculateMipCount(const uint32_t width, const uint32_t height) noexcept
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;  // +1 for base mip level.
}

}  // namespace TextureUtils

// TODO: Parallel texture loading/compressing.
#if 0
enum class ETextureLoadType : uint8_t
{
    TEXTURE_LOAD_TYPE_ALBEDO,
    TEXTURE_LOAD_TYPE_NORMAL,
    TEXTURE_LOAD_TYPE_METALLIC_ROUGHNESS,
    TEXTURE_LOAD_TYPE_EMISSIVE,
    TEXTURE_LOAD_TYPE_OCCLUSION
};
class Submesh;
#endif

class TextureManager final
{
  public:
    // NOTE:
    // Compresses data from srcImageFormat into textureSpec.Format
    // outImageData will be fullfiled, so you have to free() it manually.
    static void Compress(TextureSpecification& textureSpec, const EImageFormat srcImageFormat, const void* rawImageData,
                         const size_t rawImageSize, void** outImageData, size_t& outImageSize) noexcept;

    static void SaveCompressed(const std::filesystem::path& savePath, const TextureSpecification& textureSpec, const void* imageData,
                               const size_t imageSize) noexcept;

    static void LoadCompressed(const std::filesystem::path& loadPath, TextureSpecification& outTextureSpec,
                               std::vector<uint8_t>& outData) noexcept;

    static void Init() noexcept;
    static void Shutdown() noexcept;

    NODISCARD FORCEINLINE static const auto& GetWhiteTexture() noexcept { return s_WhiteTexture; }

#if 0
    template <typename Func, typename... Args>
    static Shared<Texture>& LoadTextureLazy(const ETextureLoadType textureLoadType, Weak<Submesh> submesh, const std::string& baseMeshDir,
                                            const std::string& textureName, Func&& func, Args&&... args) noexcept
    {
        // const std::string wholeTextureName = baseMeshDir + textureName;

        ////    if (loadedTextures.contains(textureName)) return loadedTextures[textureName];

        ///*baseMeshDir + textureName*/
        // s_TexturesInProcess.emplace_back(ThreadPool::Submit(std::forward<Func>(func), std::forward<Args>(args)...), submesh,
        //                                  textureLoadType);

        return s_WhiteTexture;
    }
#endif

    static void LinkLoadedTexturesWithMeshes() noexcept;

  private:
    static inline Shared<Texture> s_WhiteTexture = nullptr;

    static inline std::mutex s_CompressorMutex;

#if 0
      static inline Pool<Shared<Texture>> s_TexturePool;

   static inline UnorderedMap<std::string, uint32_t> s_TexturesInProcessNameToIndex;

   struct LoadedTextureEntry
   
       std::shared_future<Shared<Texture>> TextureFuture;
       Weak<Submesh> SubmeshPtr;
       ETextureLoadType TextureLoadType;
   };
   static inline std::vector<LoadedTextureEntry> s_TexturesInProcess;
#endif

    TextureManager()  = delete;
    ~TextureManager() = default;
};

struct SamplerSpecification
{
    ESamplerFilter MinFilter            = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    ESamplerFilter MagFilter            = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    ESamplerWrap WrapS                  = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerWrap WrapT                  = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    bool bAnisotropyEnable              = false;
    bool bCompareEnable                 = false;
    float MipLodBias                    = 0.0f;
    float MaxAnisotropy                 = 0.0f;
    float MinLod                        = 0.0f;
    float MaxLod                        = 0.0f;
    ECompareOp CompareOp                = ECompareOp::COMPARE_OP_NEVER;
    ESamplerReductionMode ReductionMode = ESamplerReductionMode::SAMPLER_REDUCTION_MODE_NONE;

    struct Hash
    {
        std::size_t operator()(const SamplerSpecification& key) const
        {
            std::size_t hash =
                std::hash<std::uint64_t>{}(static_cast<uint64_t>(key.MinFilter) + static_cast<uint64_t>(key.MagFilter) +
                                           static_cast<uint64_t>(key.WrapS) + static_cast<uint64_t>(key.WrapT) +
                                           static_cast<uint64_t>(key.CompareOp) + static_cast<uint64_t>(key.bAnisotropyEnable) +
                                           static_cast<uint64_t>(key.bCompareEnable)) +
                static_cast<uint64_t>(key.ReductionMode);

            hash ^= std::hash<int>{}(key.MipLodBias) + std::hash<int>{}(key.MaxAnisotropy) + std::hash<int>{}(key.MinLod) +
                    std::hash<int>{}(key.MaxLod);

            hash ^= 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    bool operator==(const SamplerSpecification& other) const
    {
        return std::tie(MinFilter, MagFilter, WrapS, WrapT, bAnisotropyEnable, bCompareEnable, CompareOp, MipLodBias, MaxAnisotropy, MinLod,
                        MaxLod, ReductionMode) == std::tie(other.MinFilter, other.MagFilter, other.WrapS, other.WrapT,
                                                           other.bAnisotropyEnable, other.bCompareEnable, other.CompareOp, other.MipLodBias,
                                                           other.MaxAnisotropy, other.MinLod, other.MaxLod, other.ReductionMode);
    }
};

// TODO: Remove, create better SamplerCache
class SamplerStorage : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    FORCEINLINE static void Shutdown()
    {
        for (auto& [samplerSpec, samplerPair] : s_Samplers)
            DestroySampler(samplerSpec);

        delete s_Instance;
        s_Instance = nullptr;
        LOG_TRACE("{}", __FUNCTION__);
    }

    NODISCARD FORCEINLINE static uint32_t GetSamplerCount() { return static_cast<uint32_t>(s_Samplers.size()); }

    // NOTE: Increments image usage of retrieve sampler on every invocation.
    NODISCARD FORCEINLINE static void* CreateOrRetrieveCachedSampler(const SamplerSpecification& samplerSpec)
    {
        PFR_ASSERT(s_Instance, "Invalid sampler storage instance!");
        if (auto it = s_Samplers.find(samplerSpec); it == s_Samplers.end())
        {
            s_Samplers[samplerSpec].first  = 1;
            s_Samplers[samplerSpec].second = s_Instance->CreateSamplerImpl(samplerSpec);
            return s_Samplers[samplerSpec].second;
        }

        auto& pair = s_Samplers[samplerSpec];

        ++pair.first;
        return pair.second;
    }

    // NOTE: Decrements image usage of retrieved sampler on every invocation, in the end destroys.
    FORCEINLINE static void DestroySampler(const SamplerSpecification& samplerSpec)
    {
        PFR_ASSERT(s_Instance, "Invalid sampler storage instance!");

        // Decrement image usage or destroy.
        if (auto it = s_Samplers.find(samplerSpec); it != s_Samplers.end())
        {
            if (it->second.first == 1)
            {
                s_Instance->DestroySamplerImpl(it->second.second);
                s_Samplers.erase(it);
            }
            else
                --it->second.first;
        }
    }

  protected:
    static inline SamplerStorage* s_Instance = nullptr;
    static inline UnorderedMap<SamplerSpecification, std::pair<uint32_t, void*>, SamplerSpecification::Hash>
        s_Samplers;  // cached samples, pair<NumOfImagesUsingSampler, Sampler>

    SamplerStorage()          = default;
    virtual ~SamplerStorage() = default;

    virtual void* CreateSamplerImpl(const SamplerSpecification& samplerSpec) = 0;
    virtual void DestroySamplerImpl(void* sampler)                           = 0;
};

}  // namespace Pathfinder
