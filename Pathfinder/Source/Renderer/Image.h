#ifndef IMAGE_H
#define IMAGE_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

// Mostly stolen from vulkan_core.h
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
    FORMAT_A2R10G10B10_UNORM_PACK32,

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
    uint32_t Width             = 1280;
    uint32_t Height            = 720;
    EImageFormat Format        = EImageFormat::FORMAT_UNDEFINED;
    EImageLayout Layout        = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
    ImageUsageFlags UsageFlags = 0;
    uint32_t Mips              = 1;
};

// TODO: Big refactor, add support for cube map images
class Image : private Uncopyable, private Unmovable
{
  public:
    virtual ~Image() = default;

    NODISCARD FORCEINLINE virtual const ImageSpecification& GetSpecification() const = 0;
    NODISCARD FORCEINLINE virtual void* Get() const                                  = 0;
    virtual void Resize(const uint32_t width, const uint32_t height)                 = 0;
    virtual void SetLayout(const EImageLayout newLayout)                             = 0;

    static Shared<Image> Create(const ImageSpecification& imageSpec);

  protected:
    Image() = default;

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
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
}  // namespace ImageUtils

}  // namespace Pathfinder

#endif  // IMAGE_H
