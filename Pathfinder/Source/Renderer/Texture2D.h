#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#include "Core/Core.h"

namespace Pathfinder
{

struct TextureSpecification
{
};

class Texture2D : private Uncopyable, private Unmovable
{
  public:
    Texture2D()          = default;
    virtual ~Texture2D() = default;

    NODISCARD FORCEINLINE virtual uint32_t GetBindlessIndex() const = 0;

    NODISCARD static Shared<Texture2D> Create(const TextureSpecification& textureSpec);

  protected:
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif