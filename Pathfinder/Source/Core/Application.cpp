#include "PathfinderPCH.h"
#include "Application.h"

#include "Window.h"
#include "Renderer/GraphicsContext.h"
#include "Events/Events.h"
#include "Events/WindowEvent.h"
#include "Events/KeyEvent.h"

namespace Pathfinder
{

#define PFR_BIND_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

Application::Application(const ApplicationSpecification& appSpec) noexcept : m_Specification(appSpec)
{
    s_Instance   = this;
    m_bIsRunning = true;

    Logger::Initialize();
    RendererAPI::Set(m_Specification.RendererAPI);

    m_GraphicsContext = GraphicsContext::Create(m_Specification.RendererAPI);
    m_Window = Window::Create({PFR_BIND_FN(Application::OnEvent), m_Specification.Title, m_Specification.Width, m_Specification.Height});
}

void Application::Run()
{
    LOG_ERROR("Hi world! %f", 0.5f);
    while (m_Window->IsRunning() && m_bIsRunning)
    {
        Timer t = {};
        if (!m_Window->IsMinimized())
        {
            m_Window->BeginFrame();

            // std::string title = std::string("PATHFINDER x64 ") + std::to_string(t.GetElapsedMilliseconds());
            //  m_Window->SetWindowTitle(title.data());

            m_Window->SwapBuffers();
        }

        m_Window->PollEvents();
    }
}

void Application::OnEvent(Event& e)
{
    LOG_TRACE("%s", e.Format().data());

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonPressedEvent>(
        [this](KeyButtonPressedEvent& e)
        {
            if (e.GetKey() == 256)
            {
                Close();
            }
        });
}

Application::~Application()
{
    m_Window->Destroy();
    m_GraphicsContext->Destroy();

    Logger::Shutdown();
    s_Instance = nullptr;
}

}  // namespace Pathfinder