#pragma once

#include <Core/Core.h>
#include <Core/Window.h>
#include <Renderer/RendererCoreDefines.h>
#include "Texture.h"

namespace Pathfinder
{

class CommandBuffer;

class Swapchain : private Uncopyable, private Unmovable
{
  public:
    virtual ~Swapchain() = default;

    NODISCARD virtual const EImageFormat GetImageFormat() const               = 0;
    NODISCARD FORCEINLINE virtual const uint32_t GetCurrentFrameIndex() const = 0;
    NODISCARD FORCEINLINE virtual const uint32_t GetImageCount() const        = 0;
    NODISCARD FORCEINLINE bool IsVSync() const { return m_PresentMode == EPresentMode::PRESENT_MODE_FIFO; }
    NODISCARD FORCEINLINE EPresentMode GetPresentMode() const { return m_PresentMode; }

    virtual void SetClearColor(const glm::vec3& clearColor = glm::vec3(1.0f)) = 0;
    virtual void SetVSync(bool bVSync)                                        = 0;
    virtual void SetWindowMode(const EWindowMode windowMode)                  = 0;
    virtual void SetPresentMode(const EPresentMode presentMode)               = 0;

    virtual void BeginPass(const Shared<CommandBuffer>& commandBuffer, const bool bPreserveContents = true) = 0;
    virtual void EndPass(const Shared<CommandBuffer>& commandBuffer)                                        = 0;

    virtual bool AcquireImage() = 0;
    virtual void PresentImage() = 0;

    NODISCARD FORCEINLINE virtual void* GetImageAvailableSemaphore() const = 0;
    NODISCARD FORCEINLINE virtual void* GetRenderFence() const             = 0;
    NODISCARD FORCEINLINE virtual void* GetRenderSemaphore() const         = 0;

    virtual void Invalidate()                                       = 0;
    virtual void CopyToSwapchain(const Shared<Texture>& texture)        = 0;
    virtual void AddResizeCallback(ResizeCallback&& resizeCallback) = 0;

    static Unique<Swapchain> Create(void* windowHandle);

  protected:
    EPresentMode m_PresentMode = EPresentMode::PRESENT_MODE_FIFO;

    std::vector<ResizeCallback> m_ResizeCallbacks;

    Swapchain() noexcept   = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

