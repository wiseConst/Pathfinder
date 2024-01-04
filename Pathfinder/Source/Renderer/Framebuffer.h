#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Core/Core.h"

namespace Pathfinder
{

struct FramebufferSpecification
{
};

class Framebuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Framebuffer() = default;

    NODISCARD FORCEINLINE virtual const FramebufferSpecification& GetSpecification() = 0;

    // TODO: Revisit with Unique/Shared approach
    NODISCARD static Shared<Framebuffer> Create(const FramebufferSpecification& framebufferSpec);

  protected:
    Framebuffer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // FRAMEBUFFER_H
