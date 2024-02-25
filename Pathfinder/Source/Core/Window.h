#ifndef WINDOW_H
#define WINDOW_H

#include "Core.h"
#include "Math.h"
#include <functional>
#include "Keys.h"

namespace Pathfinder
{

class Swapchain;
class Event;

enum class EWindowMode : uint8_t
{
    WINDOW_MODE_WINDOWED = 0,
    WINDOW_MODE_BORDERLESS_FULLSCREEN,
    WINDOW_MODE_FULLSCREEN_EXCLUSIVE
};

struct WindowSpecification final
{
    using EventFn = std::function<void(Event&)>;
    const EventFn EventCallback;
    std::string_view Title = "PATHFINDER";
    uint32_t Width         = 1280;
    uint32_t Height        = 720;
    EWindowMode WindowMode = EWindowMode::WINDOW_MODE_WINDOWED;
};

using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

class Framebuffer;
class Image;
class Window : private Uncopyable, private Unmovable
{
  public:
    virtual ~Window();

    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE const auto& GetSwapchain() const { return m_Swapchain; }
    NODISCARD FORCEINLINE virtual const WindowSpecification& GetSpecification() const = 0;
    NODISCARD FORCEINLINE virtual const uint32_t GetCurrentFrameIndex() const         = 0;

    virtual void SetClearColor(const glm::vec3& clearColor = glm::vec3(1.0f)) = 0;
    virtual void SetVSync(bool bVSync)                                        = 0;
    virtual void SetWindowMode(const EWindowMode windowMode)                  = 0;
    virtual void SetWindowTitle(const char* title)                            = 0;

    static std::vector<const char*> GetWSIExtensions();  // implemented in derived
    FORCEINLINE virtual bool IsMinimized() const = 0;
    FORCEINLINE virtual bool IsRunning() const   = 0;

    virtual void BeginFrame()                                         = 0;
    virtual void SwapBuffers()                                        = 0;
    virtual void PollEvents()                                         = 0;
    virtual void CopyToWindow(const Shared<Framebuffer>& framebuffer) = 0;
    virtual void CopyToWindow(const Shared<Image>& image)             = 0;

    virtual void AddResizeCallback(ResizeCallback&& resizeCallback) = 0;

    static Unique<Window> Create(const WindowSpecification& windowSpec = {});

  protected:
    Unique<Swapchain> m_Swapchain;

    explicit Window() noexcept = default;
    virtual void Destroy()     = 0;
};

}  // namespace Pathfinder

#endif  // WINDOW_H
