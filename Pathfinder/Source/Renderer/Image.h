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

struct ImageSpecification
{
    uint32_t Width      = 1280;
    uint32_t Height     = 720;
    EImageLayout Layout = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
};

class Image : private Uncopyable, private Unmovable
{
  public:
    virtual ~Image() = default;

    static Unique<Image> Create(const ImageSpecification& imageSpec);

  protected:
    Image()                = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // IMAGE_H
