#pragma once

#include <Core/Core.h>
#include "RendererCoreDefines.h"

namespace Pathfinder
{


// NOTE: Bindless by default, once and forever.
struct ImageSpecification
{
    std::string DebugName      = s_DEFAULT_STRING;
    uint32_t Width             = 0;
    uint32_t Height            = 0;
    EImageFormat Format        = EImageFormat::FORMAT_UNDEFINED;
    EImageLayout Layout        = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
    ESamplerWrap Wrap          = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter      = ESamplerFilter::SAMPLER_FILTER_NEAREST;
    ImageUsageFlags UsageFlags = 0;
    uint32_t Mips              = 1;
    uint32_t Layers            = 1;
};

class Image : private Uncopyable, private Unmovable
{
  public:
    virtual ~Image() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE const auto GetBindlessIndex() const
    {
        PFR_ASSERT(m_BindlessIndex.has_value(), "Image doesn't have bindless index!");
        return m_BindlessIndex.value();
    }

    virtual void Resize(const uint32_t width, const uint32_t height)                                  = 0;
    virtual void SetLayout(const EImageLayout newLayout)                                              = 0;
    virtual void SetData(const void* data, size_t dataSize)                                           = 0;
    virtual void ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const = 0;

    static Shared<Image> Create(const ImageSpecification& imageSpec);

    const auto& GetUUID() const { return m_UUID; }

  protected:
    ImageSpecification m_Specification      = {};
    UUID m_UUID                             = {};
    std::optional<uint32_t> m_BindlessIndex = std::nullopt;

    Image(const ImageSpecification& imageSpec) : m_Specification(imageSpec) {}
    Image() = delete;

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

struct SamplerSpecification
{
    ESamplerFilter Filter  = ESamplerFilter::SAMPLER_FILTER_LINEAR;
    ESamplerWrap Wrap      = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    bool bAnisotropyEnable = false;
    bool bCompareEnable    = false;
    float MipLodBias       = 0.0f;
    float MaxAnisotropy    = 0.0f;
    float MinLod           = 0.0f;
    float MaxLod           = 0.0f;
    ECompareOp CompareOp   = ECompareOp::COMPARE_OP_NEVER;

    struct Hash
    {
        std::size_t operator()(const SamplerSpecification& key) const
        {
            std::size_t hash = std::hash<std::uint64_t>{}(
                static_cast<uint64_t>(key.Filter) + static_cast<uint64_t>(key.Wrap) + static_cast<uint64_t>(key.CompareOp) +
                static_cast<uint64_t>(key.bAnisotropyEnable) + static_cast<uint64_t>(key.bCompareEnable));

            hash ^= std::hash<int>{}(key.MipLodBias) + std::hash<int>{}(key.MaxAnisotropy) + std::hash<int>{}(key.MinLod) +
                    std::hash<int>{}(key.MaxLod);

            hash ^= 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    bool operator==(const SamplerSpecification& other) const
    {
        return std::tie(Filter, Wrap, bAnisotropyEnable, bCompareEnable, CompareOp, MipLodBias, MaxAnisotropy, MinLod, MaxLod) ==
               std::tie(other.Filter, other.Wrap, other.bAnisotropyEnable, other.bCompareEnable, other.CompareOp, other.MipLodBias,
                        other.MaxAnisotropy, other.MinLod, other.MaxLod);
    }
};

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
            if (it->second.first == 1 || it->second.first == 0)
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
    static inline std::unordered_map<SamplerSpecification, std::pair<uint32_t, void*>, SamplerSpecification::Hash>
        s_Samplers;  // cached samples, pair<NumOfImagesUsingSampler, Sampler>

    SamplerStorage()          = default;
    virtual ~SamplerStorage() = default;

    virtual void* CreateSamplerImpl(const SamplerSpecification& samplerSpec) = 0;
    virtual void DestroySamplerImpl(void* sampler)                           = 0;
};

namespace ImageUtils
{

static bool IsDepthFormat(const EImageFormat imageFormat)
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

static bool IsStencilFormat(const EImageFormat imageFormat)
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

void* LoadRawImage(const std::filesystem::path& imagePath, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels);

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels);

void* ConvertRgbToRgba(const uint8_t* rgb, const uint32_t width, const uint32_t height);

void UnloadRawImage(void* data);

FORCEINLINE static uint32_t CalculateMipCount(const uint32_t width, const uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;  // +1 for base mip level.
}

}  // namespace ImageUtils

}  // namespace Pathfinder
