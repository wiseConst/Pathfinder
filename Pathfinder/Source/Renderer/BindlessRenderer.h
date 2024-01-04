#ifndef BINDLESSRENDERER_H
#define BINDLESSRENDERER_H

#include "Core/Core.h"

namespace Pathfinder
{

class BindlessRenderer : private Uncopyable, private Unmovable
{
  public:
    virtual ~BindlessRenderer() = default;
    NODISCARD static Unique<BindlessRenderer> Create();

  protected:
    BindlessRenderer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BINDLESSRENDERER_H
