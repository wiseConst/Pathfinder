#ifndef IMAGE_H
#define IMAGE_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

enum class EImageLayout : uint8_t
{
    IMAGE_LAYOUT_UNDEFINED = 0,
    IMAGE_LAYOUT_GENERAL,
    IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_PRESENT_SRC,
    IMAGE_LAYOUT_SHARED_PRESENT,
    IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL
};

enum EImageUsage : uint32_t
{
    IMAGE_USAGE_TRANSFER_SRC_BIT                     = BIT(0),
    IMAGE_USAGE_TRANSFER_DST_BIT                     = BIT(1),
    IMAGE_USAGE_SAMPLED_BIT                          = BIT(2),
    IMAGE_USAGE_STORAGE_BIT                          = BIT(3),
    IMAGE_USAGE_COLOR_ATTACHMENT_BIT                 = BIT(4),
    IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT         = BIT(5),
    IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT             = BIT(6),
    IMAGE_USAGE_INPUT_ATTACHMENT_BIT                 = BIT(7),
    IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = BIT(8),
    IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT             = BIT(9),
};
typedef uint32_t ImageUsageFlags;

enum class EImageFormat : uint8_t
{
    FORMAT_UNDEFINED = 0,

    FORMAT_RGB8_UNORM,
    FORMAT_RGBA8_UNORM,
    FORMAT_BGRA8_UNORM,  // Swapchain
    FORMAT_A2R10G10B10_UNORM_PACK32,

    FORMAT_R8_UNORM,

    FORMAT_R16_UNORM,
    FORMAT_R16F,

    FORMAT_R32F,
    FORMAT_R64F,

    FORMAT_RGB16_UNORM,
    FORMAT_RGB16F,

    FORMAT_RGBA16_UNORM,
    FORMAT_RGBA16F,

    FORMAT_RGB32F,
    FORMAT_RGBA32F,

    FORMAT_RGB64F,
    FORMAT_RGBA64F,

    // DEPTH
    FORMAT_D16_UNORM,
    FORMAT_D32F,
    FORMAT_S8_UINT,
    FORMAT_D16_UNORM_S8_UINT,
    FORMAT_D24_UNORM_S8_UINT,
};

struct ImageSpecification
{
    uint32_t Width             = 0;
    uint32_t Height            = 0;
    EImageFormat Format        = EImageFormat::FORMAT_UNDEFINED;
    EImageLayout Layout        = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
    ESamplerWrap Wrap          = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter      = ESamplerFilter::SAMPLER_FILTER_NEAREST;
    ImageUsageFlags UsageFlags = 0;
    uint32_t Mips              = 1;
    uint32_t Layers            = 1;
    bool bBindlessUsage        = false;
};

class Image : private Uncopyable, private Unmovable
{
  public:
    virtual ~Image() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const { return m_Index; }

    virtual void Resize(const uint32_t width, const uint32_t height)                                  = 0;
    virtual void SetLayout(const EImageLayout newLayout)                                              = 0;
    virtual void SetData(const void* data, size_t dataSize)                                           = 0;
    virtual void ClearColor(const Shared<CommandBuffer>& commandBuffer, const glm::vec4& color) const = 0;

    static Shared<Image> Create(const ImageSpecification& imageSpec);

    const auto& GetUUID() const { return m_UUID; }

  protected:
    ImageSpecification m_Specification = {};
    uint32_t m_Index                   = UINT32_MAX;  // bindless array purposes
    UUID m_UUID                        = {};

    Image(const ImageSpecification& imageSpec) : m_Specification(imageSpec) {}
    Image() = default;

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
        if (Filter == other.Filter &&                        //
            Wrap == other.Wrap &&                            //
            bAnisotropyEnable == other.bAnisotropyEnable &&  //
            bCompareEnable == other.bCompareEnable &&        //
            CompareOp == other.CompareOp &&                  //
            MipLodBias == other.MipLodBias &&                //
            MaxAnisotropy == other.MaxAnisotropy &&          //
            MinLod == other.MinLod &&                        //
            MaxLod == other.MaxLod)
        {
            return true;
        }

        return false;
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
        case EImageFormat::FORMAT_D16_UNORM: return true;
        case EImageFormat::FORMAT_D32F: return true;
        case EImageFormat::FORMAT_S8_UINT: return true;
        case EImageFormat::FORMAT_D24_UNORM_S8_UINT: return true;
        case EImageFormat::FORMAT_D16_UNORM_S8_UINT: return true;
        default: return false;
    }

    PFR_ASSERT(false, "Unknown image format!");
    return false;
}

void* LoadRawImage(std::string_view path, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels);

void* LoadRawImageFromMemory(const uint8_t* data, size_t dataSize, bool bFlipOnLoad, int32_t* x, int32_t* y, int32_t* nChannels);

void UnloadRawImage(void* data);

}  // namespace ImageUtils

}  // namespace Pathfinder

#endif  // IMAGE_H
