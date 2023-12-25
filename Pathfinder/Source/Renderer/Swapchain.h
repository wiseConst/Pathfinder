#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "Core/Core.h"

namespace Pathfinder
{

class Swapchain : private Uncopyable, private Unmovable
{
  public:
    Swapchain() noexcept = default;
    virtual ~Swapchain() = default;

    virtual void AcquireImage() = 0;
    virtual void PresentImage() = 0;

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;

    static Unique<Swapchain> Create();
};

}  // namespace Pathfinder

#endif  // SWAPCHAIN_H
