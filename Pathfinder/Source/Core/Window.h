#ifndef WINDOW_H
#define WINDOW_H

#include "Core/Core.h"
#include <functional>

namespace Pathfinder
{

class Swapchain;
class Event;

struct WindowSpecification final
{
    using EventFn = std::function<void(Event&)>;
    const EventFn EventCallback;
    std::string_view Title = "PATHFINDER";
    uint32_t Width         = 1280;
    uint32_t Height        = 720;
};

class Window : private Uncopyable, private Unmovable
{
  public:
    explicit Window() noexcept = default;
    virtual ~Window();

    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }

    static std::vector<const char*> GetWSIExtensions();
    FORCEINLINE virtual bool IsMinimized() const   = 0;
    FORCEINLINE virtual bool IsRunning() const     = 0;
    virtual void SetWindowTitle(const char* title) = 0;
    virtual void BeginFrame()                      = 0;
    virtual void SwapBuffers()                     = 0;
    virtual void PollEvents()                      = 0;
    virtual void Destroy()                         = 0;

    static Unique<Window> Create(const WindowSpecification& windowSpec = {});

  protected:
    WindowSpecification m_Specification;
    Unique<Swapchain> m_Swapchain;
};

}  // namespace Pathfinder

#endif  // WINDOW_H
