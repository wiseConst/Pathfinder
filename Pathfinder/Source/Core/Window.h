#ifndef WINDOW_H
#define WINDOW_H

#include "Core.h"
#include "Math.h"
#include "Keys.h"

#include <functional>

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

enum class EPresentMode : uint8_t
{
    PRESENT_MODE_FIFO = 0,   // VSync on.
    PRESENT_MODE_IMMEDIATE,  // VSync off.
    PRESENT_MODE_MAILBOX,    // Triple-buffered.
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
    NODISCARD FORCEINLINE const WindowSpecification& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual const uint32_t GetCurrentFrameIndex() const = 0;
    bool IsVSync() const;

    NODISCARD FORCEINLINE bool IsRunning() const { return m_bIsRunning; }
    NODISCARD FORCEINLINE bool IsMinimized() const { return m_Specification.Height == 0 || m_Specification.Width == 0; }

    virtual void SetClearColor(const glm::vec3& clearColor = glm::vec3(1.0f)) = 0;
    void SetVSync(const bool bVSync);
    void SetPresentMode(const EPresentMode presentMode);
    virtual void SetWindowMode(const EWindowMode windowMode) = 0;
    virtual void SetWindowTitle(const char* title)           = 0;

    static std::vector<const char*> GetWSIExtensions();  // implemented in derived classes only

    virtual bool BeginFrame()                             = 0;
    virtual void SwapBuffers()                            = 0;
    virtual void PollEvents()                             = 0;
    virtual void CopyToWindow(const Shared<Image>& image) = 0;

    virtual void AddResizeCallback(ResizeCallback&& resizeCallback) = 0;

    static Unique<Window> Create(const WindowSpecification& windowSpec = {});

  protected:
    Unique<Swapchain> m_Swapchain;
    WindowSpecification m_Specification;
    bool m_bIsRunning = true;

    explicit Window(const WindowSpecification& windowSpec) noexcept;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // WINDOW_H
