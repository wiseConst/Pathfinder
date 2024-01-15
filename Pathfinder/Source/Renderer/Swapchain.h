#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "Core/Core.h"
#include "Core/Window.h"
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{

class Swapchain : private Uncopyable, private Unmovable
{
  public:
    virtual ~Swapchain() = default;

    NODISCARD FORCEINLINE virtual const uint32_t GetCurrentFrameIndex() const = 0;
    NODISCARD FORCEINLINE virtual bool WasInvalidatedDuringCurrentFrame() const = 0;

    virtual void SetClearColor(const glm::vec3& clearColor = glm::vec3(1.0f)) = 0;
    virtual void SetVSync(bool bVSync)                                        = 0;
    virtual void SetWindowMode(const EWindowMode windowMode)                  = 0;

    virtual void AcquireImage() = 0;
    virtual void PresentImage() = 0;

    virtual void Invalidate()                                       = 0;
    virtual void CopyToSwapchain(const Shared<Image>& image)        = 0;
    virtual void AddResizeCallback(ResizeCallback&& resizeCallback) = 0;

    static Unique<Swapchain> Create(void* windowHandle);

  protected:
    Swapchain() noexcept   = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // SWAPCHAIN_H
