#ifndef GLFWWINDOW_H
#define GLFWWINDOW_H

#include "Core/Window.h"

struct GLFWwindow;

namespace Pathfinder
{

class GLFWWindow final : public Window
{
  public:
    explicit GLFWWindow(const WindowSpecification& windowSpec) noexcept;
    ~GLFWWindow() override = default;

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }

    FORCEINLINE bool IsRunning() const final override { return m_bIsRunning; }
    FORCEINLINE bool IsMinimized() const final override { return m_Specification.Height == 0 && m_Specification.Width == 0; }
    void SetWindowTitle(const char* title) final override;
    void BeginFrame() final override;
    void SwapBuffers() final override;
    void PollEvents() final override;
    void Destroy() final override;

  private:
    WindowSpecification m_Specification;
    GLFWwindow* m_Handle = nullptr;
    bool m_bIsRunning    = true;

    void SetEventCallbacks() const;
};

}  // namespace Pathfinder

#endif  // GLFWWINDOW_H
