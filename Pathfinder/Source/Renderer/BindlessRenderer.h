#ifndef BINDLESSRENDERER_H
#define BINDLESSRENDERER_H

#include "RendererCoreDefines.h"
#include "Core/Core.h"

namespace Pathfinder
{

class CommandBuffer;

class BindlessRenderer : private Uncopyable, private Unmovable
{
  public:
    virtual ~BindlessRenderer() = default;

    virtual void Bind(const Shared<CommandBuffer>& commandBuffer, const bool bGraphicsBindPoint = true) = 0;

    virtual void LoadImage(const ImagePerFrame& images) = 0;
    virtual void LoadImage(const Shared<Image>& image)  = 0;

    virtual void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) = 0;

    NODISCARD static Shared<BindlessRenderer> Create();

  protected:
    BindlessRenderer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BINDLESSRENDERER_H
