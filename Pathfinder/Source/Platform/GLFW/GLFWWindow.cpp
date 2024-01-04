#include "PathfinderPCH.h"
#include "GLFWWindow.h"
#include <GLFW/glfw3.h>

#include "Renderer/Swapchain.h"

#include "Core/Application.h"
#include "Events/WindowEvent.h"
#include "Events/MouseEvent.h"
#include "Events/KeyEvent.h"

namespace Pathfinder
{

static void InitGLFW()
{
    static bool s_bIsGLFWInitialized = false;

    if (!s_bIsGLFWInitialized)
    {
        PFR_ASSERT(glfwInit() == GLFW_TRUE, "Failed to initialize GLFW!");

        glfwSetErrorCallback([](const int error_code, const char* description)
                             { LOG_ERROR("Error Code: %d\nDescription:%s\n", error_code, description); });
        s_bIsGLFWInitialized = true;
    }
}

Window::~Window() = default;

Unique<Window> Window::Create(const WindowSpecification& windowSpec)
{
    return MakeUnique<GLFWWindow>(windowSpec);
}

std::vector<const char*> Window::GetWSIExtensions()
{
    InitGLFW();

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    PFR_ASSERT(glfwExtensionCount != 0, "glfwExtensions are empty!");

    std::vector<const char*> wsiExtensions;
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        wsiExtensions.push_back(glfwExtensions[i]);

    return wsiExtensions;
}

GLFWWindow::GLFWWindow(const WindowSpecification& windowSpec) noexcept : m_Specification(windowSpec)
{
    InitGLFW();

    switch (Application::Get().GetSpecification().RendererAPI)
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            PFR_ASSERT(glfwVulkanSupported() == GLFW_TRUE, "GLFW WSI Vulkan not supported!");
            break;
        }
    }
    m_Handle = glfwCreateWindow(static_cast<int32_t>(m_Specification.Width), static_cast<int32_t>(m_Specification.Height),
                                m_Specification.Title.data(), nullptr, nullptr);
    PFR_ASSERT(m_Handle, "Failed to create window!");

    m_Swapchain = Swapchain::Create(m_Handle);

    glfwSetWindowUserPointer(m_Handle, this);
    SetEventCallbacks();

    LOG_TAG_TRACE(GLFW, "Created window \"%s\" (%u, %u).", m_Specification.Title.data(), m_Specification.Width, m_Specification.Height);
}

void GLFWWindow::BeginFrame()
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    m_Swapchain->AcquireImage();
}

const uint32_t GLFWWindow::GetCurrentFrameIndex() const
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    return m_Swapchain->GetCurrentFrameIndex();
}

void GLFWWindow::SetClearColor(const glm::vec3& clearColor)
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    m_Swapchain->SetClearColor(clearColor);
}

void GLFWWindow::SetVSync(bool bVSync)
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    m_Swapchain->SetVSync(bVSync);
}

void GLFWWindow::SetWindowMode(const EWindowMode windowMode)
{
    if (m_Specification.WindowMode == windowMode) return;

    m_Specification.WindowMode = windowMode;
    m_Swapchain->SetWindowMode(windowMode);
    m_Swapchain->Invalidate();
}

void GLFWWindow::SetWindowTitle(const char* title)
{
    if (!title) return;
    glfwSetWindowTitle(m_Handle, title);
}

void GLFWWindow::SwapBuffers()
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    m_Swapchain->PresentImage();
}

void GLFWWindow::PollEvents()
{
    IsMinimized() ? glfwWaitEvents() : glfwPollEvents();
}

void GLFWWindow::SetEventCallbacks() const
{
    glfwSetWindowCloseCallback(m_Handle,
                               [](GLFWwindow* window)
                               {
                                   void* data = glfwGetWindowUserPointer(window);
                                   PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                                   const auto userWindow = static_cast<GLFWWindow*>(data);
                                   PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");
                                   userWindow->m_bIsRunning = false;

                                   WindowCloseEvent e = {};
                                   userWindow->m_Specification.EventCallback(e);
                               });

    glfwSetWindowSizeCallback(m_Handle,
                              [](GLFWwindow* window, int width, int height)
                              {
                                  void* data = glfwGetWindowUserPointer(window);
                                  PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                                  auto userWindow = static_cast<GLFWWindow*>(data);
                                  PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");
                                  userWindow->m_Specification.Height = height;
                                  userWindow->m_Specification.Width  = width;

                                  WindowResizeEvent e(width, height);
                                  userWindow->m_Specification.EventCallback(e);

                                  //  userWindow->m_Swapchain->Invalidate(); // I can't do this since, width and height can be zero,
                                  //  swapchain doesn't like it
                              });

    glfwSetCursorPosCallback(m_Handle,
                             [](GLFWwindow* window, double xpos, double ypos)
                             {
                                 void* data = glfwGetWindowUserPointer(window);
                                 PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                                 const auto userWindow = static_cast<GLFWWindow*>(data);
                                 PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");

                                 MouseMovedEvent e(static_cast<uint32_t>(xpos), static_cast<uint32_t>(ypos));
                                 userWindow->m_Specification.EventCallback(e);
                             });

    glfwSetScrollCallback(m_Handle,
                          [](GLFWwindow* window, double xpos, double ypos)
                          {
                              void* data = glfwGetWindowUserPointer(window);
                              PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                              const auto userWindow = static_cast<GLFWWindow*>(data);
                              PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");

                              MouseScrolledEvent e(xpos, ypos);
                              userWindow->m_Specification.EventCallback(e);
                          });

    glfwSetMouseButtonCallback(m_Handle,
                               [](GLFWwindow* window, int button, int action, int mods)
                               {
                                   void* data = glfwGetWindowUserPointer(window);
                                   PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                                   const auto userWindow = static_cast<GLFWWindow*>(data);
                                   PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");

                                   switch (action)
                                   {
                                       case GLFW_PRESS:
                                       {
                                           MouseButtonPressedEvent e(button);
                                           userWindow->m_Specification.EventCallback(e);
                                           break;
                                       }
                                       case GLFW_REPEAT:
                                       {
                                           break;
                                       }
                                       case GLFW_RELEASE:
                                       {
                                           MouseButtonReleasedEvent e(button);
                                           userWindow->m_Specification.EventCallback(e);
                                           break;
                                       }
                                   }
                               });

    //   glfwSetCharCallback(m_Handle,[](){});
    glfwSetKeyCallback(m_Handle,
                       [](GLFWwindow* window, int key, int scancode, int action, int mods)
                       {
                           void* data = glfwGetWindowUserPointer(window);
                           PFR_ASSERT(data, "Failed to retrieve window user pointer!");

                           const auto userWindow = static_cast<GLFWWindow*>(data);
                           PFR_ASSERT(userWindow, "Failed to retrieve window data from window user pointer!");

                           switch (action)
                           {
                               case GLFW_PRESS:
                               {
                                   KeyButtonPressedEvent e(key, scancode);
                                   userWindow->m_Specification.EventCallback(e);
                                   break;
                               }
                               case GLFW_REPEAT:
                               {
                                   KeyButtonRepeatedEvent e(key, scancode);
                                   userWindow->m_Specification.EventCallback(e);
                                   break;
                               }
                               case GLFW_RELEASE:
                               {
                                   KeyButtonReleasedEvent e(key, scancode);
                                   userWindow->m_Specification.EventCallback(e);
                                   break;
                               }
                           }
                       });
}

void GLFWWindow::CopyToWindow(const Shared<Framebuffer>& framebuffer)
{
    PFR_ASSERT(m_Swapchain, "Swapchain is not valid!");
    m_Swapchain->CopyToSwapchain(framebuffer);
}

void GLFWWindow::Destroy()
{
    m_Swapchain.reset();

    glfwDestroyWindow(m_Handle);
    glfwTerminate();

    LOG_TAG_TRACE(GLFW, "Destroyed window \"%s\".", m_Specification.Title.data());
}

}  // namespace Pathfinder
