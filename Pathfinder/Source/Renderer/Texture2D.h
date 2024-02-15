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
};

class Texture2D : private Uncopyable, private Unmovable
{
  public:
    Texture2D()          = default;
    virtual ~Texture2D() = default;

    NODISCARD FORCEINLINE virtual const TextureSpecification& GetSpecification() const = 0;
    NODISCARD FORCEINLINE virtual uint32_t GetBindlessIndex() const                    = 0;

    NODISCARD static Shared<Texture2D> Create(const TextureSpecification& textureSpec);

  protected:
    virtual void Destroy()    = 0;
    virtual void Invalidate() = 0;
};

}  // namespace Pathfinder

#endif