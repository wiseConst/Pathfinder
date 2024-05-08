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
    const uint32_t GetCurrentFrameIndex() const final override;

    void SetClearColor(const glm::vec3& clearColor) final override;
    void SetWindowMode(const EWindowMode windowMode) final override;
    void SetWindowTitle(const std::string_view& title) final override;
    void SetIconImage(const std::string_view& iconFilePath) final override;

    void AddResizeCallback(ResizeCallback&& resizeCallback) final override;

  private:
    GLFWwindow* m_Handle = nullptr;

    void SetEventCallbacks() const;
    void CopyToWindow(const Shared<Image>& image) final override;

    bool BeginFrame() final override;
    void SwapBuffers() final override;
    void PollEvents() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // GLFWWINDOW_H
