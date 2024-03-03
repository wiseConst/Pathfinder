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
    ~GLFWWindow() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE const WindowSpecification& GetSpecification() const final override { return m_Specification; }
    const uint32_t GetCurrentFrameIndex() const final override;

    FORCEINLINE bool IsRunning() const final override { return m_bIsRunning; }
    FORCEINLINE bool IsMinimized() const final override { return m_Specification.Height == 0 || m_Specification.Width == 0; }

    void SetClearColor(const glm::vec3& clearColor) final override;
    void SetVSync(bool bVSync) final override;
    void SetWindowMode(const EWindowMode windowMode) final override;
    void SetWindowTitle(const char* title) final override;
    void AddResizeCallback(ResizeCallback&& resizeCallback) final override;

  private:
    WindowSpecification m_Specification = {};
    GLFWwindow* m_Handle                = nullptr;
    bool m_bIsRunning                   = true;

    void SetEventCallbacks() const;
    void CopyToWindow(const Shared<Image>& image) final override;

    bool BeginFrame() final override;
    void SwapBuffers() final override;
    void PollEvents() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // GLFWWINDOW_H
