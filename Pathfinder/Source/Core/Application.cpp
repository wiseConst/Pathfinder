#include "PathfinderPCH.h"
#include "Application.h"

#include "Window.h"
#include "Renderer/GraphicsContext.h"
#include "Renderer/Renderer.h"

#include "Events/Events.h"
#include "Events/WindowEvent.h"
#include "Events/KeyEvent.h"

namespace Pathfinder
{

#define PFR_BIND_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

Application::Application(const ApplicationSpecification& appSpec) noexcept : m_Specification(appSpec)
{
    s_Instance   = this;
    s_bIsRunning = true;

    Logger::Init();
    PFR_ASSERT(s_WORKER_THREAD_COUNT > 0 && Threading::GetNumThreads() > 0, "No worker threads found!");

    RendererAPI::Set(m_Specification.RendererAPI);
    m_LayerQueue = MakeUnique<LayerQueue>();

    m_GraphicsContext = GraphicsContext::Create(m_Specification.RendererAPI);
    m_Window = Window::Create({PFR_BIND_FN(Application::OnEvent), m_Specification.Title, m_Specification.Width, m_Specification.Height});

    Renderer::Init();
}

void Application::Run()
{
    m_LayerQueue->Init();

    LOG_ERROR("Hi world! %f", 0.5f);
    LOG_TRACE("Current working directory: %s", std::filesystem::current_path().string().data());
    while (m_Window->IsRunning() && s_bIsRunning)
    {
        Timer t = {};
        if (!m_Window->IsMinimized())
        {
            Renderer::Begin();

            m_Window->BeginFrame();
            m_Window->SetClearColor(glm::vec3(0.15f));

            // std::string title = std::string("PATHFINDER x64 ") + std::to_string(t.GetElapsedMilliseconds());
            //  m_Window->SetWindowTitle(title.data());

            Renderer::Flush();
            m_Window->SwapBuffers();
        }

        m_Window->PollEvents();
    }
}

void Application::OnEvent(Event& e)
{
    // LOG_TRACE("%s", e.Format().data());

    m_LayerQueue->OnEvent(e);

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonPressedEvent>(
        [this](const auto& event)
        {
            const auto key = event.GetKey();

            // if (key == EKey::KEY_ESCAPE)
            // {
            //     Close();
            // }

            if (key == EKey::KEY_F1)
            {
                m_Window->SetWindowMode(EWindowMode::WINDOW_MODE_WINDOWED);

                return true;
            }
            if (key == EKey::KEY_F2)
            {
                m_Window->SetWindowMode(EWindowMode::WINDOW_MODE_BORDERLESS_FULLSCREEN);

                return true;
            }
            if (key == EKey::KEY_F3)
            {
                m_Window->SetWindowMode(EWindowMode::WINDOW_MODE_FULLSCREEN_EXCLUSIVE);

                return true;
            }

            if (key == EKey::KEY_F4)
            {
                m_Window->SetVSync(true);

                return true;
            }

            if (key == EKey::KEY_F5)
            {
                m_Window->SetVSync(false);

                return true;
            }

            return false;
        });
}

Application::~Application()
{
    m_LayerQueue.reset();

    Renderer::Shutdown();
    m_Window.reset();
    m_GraphicsContext.reset();

    Logger::Shutdown();
    s_Instance = nullptr;
}

}  // namespace Pathfinder