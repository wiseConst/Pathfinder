#include "PathfinderPCH.h"
#include "Application.h"

#include "Threading.h"

#include "Window.h"
#include "Renderer/GraphicsContext.h"
#include "Renderer/Texture2D.h"
#include "Renderer/Renderer.h"
#include "Renderer/Renderer2D.h"

#include "Input.h"
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

    if (m_Specification.WorkingDir == s_DEFAULT_STRING) m_Specification.WorkingDir = std::filesystem::current_path().string();

    Logger::Init(std::string(m_Specification.Title) + ".log");
    PFR_ASSERT(s_FRAMES_IN_FLIGHT != 0, "Frames in flight should be greater than zero!");
    PFR_ASSERT(s_WORKER_THREAD_COUNT > 0 && JobSystem::GetNumThreads() > 0, "No worker threads found!");
    PFR_ASSERT(!m_Specification.AssetsDir.empty() && !m_Specification.MeshDir.empty() && !m_Specification.CacheDir.empty() &&
                   !m_Specification.ShadersDir.empty(),
               "Application specification doesn't have needed directories. (in e.g. AssetsDir)");

    JobSystem::Init();

    RendererAPI::Set(m_Specification.RendererAPI);
    m_LayerQueue = MakeUnique<LayerQueue>();

    m_GraphicsContext = GraphicsContext::Create(m_Specification.RendererAPI);
    m_Window = Window::Create({PFR_BIND_FN(Application::OnEvent), m_Specification.Title, m_Specification.Width, m_Specification.Height});

    Renderer::Init();

    if (m_Specification.bEnableImGui) m_UILayer = UILayer::Create();
}

void Application::Run()
{
    m_LayerQueue->Init();

    uint32_t frameCount     = 0;
    double accumulatedDelta = 0.0;

    LOG_ERROR("Hi world! %f", 0.5f);
    LOG_TRACE("Current working directory: %s", m_Specification.WorkingDir.data());
    while (m_Window->IsRunning() && s_bIsRunning)
    {
        Timer t = {};
        if (!m_Window->IsMinimized() && m_Window->BeginFrame())
        {
            Renderer::Begin();
            m_Window->SetClearColor(glm::vec3(0.15f));

            m_LayerQueue->OnUpdate(m_DeltaTime);

            if (m_Specification.bEnableImGui)
            {
                m_UILayer->BeginRender();

                m_LayerQueue->OnUIRender();
            }

            Renderer::Flush(m_UILayer);
            m_Window->SwapBuffers();
        }

        m_Window->PollEvents();
        m_DeltaTime = static_cast<float>(t.GetElapsedSeconds());

        ++frameCount;
        accumulatedDelta += m_DeltaTime;
        if (accumulatedDelta >= 1.0)
        {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(4);  // Set the float format
            ss << m_Window->GetSpecification().Title << " x64 / " << frameCount << " FPS ";
            ss << "[cpu]: " << t.GetElapsedMilliseconds() << "ms ";
            ss << "[gpu]: " << Renderer::GetStats().GPUTime + Renderer2D::GetStats().GPUTime << "ms ";
            ss << "[present]: " << Renderer::GetStats().SwapchainPresentTime << "ms ";
            ss << "[rhi]: " << Renderer::GetStats().RHITime << "ms ";
            ss << " [tris]: " << Renderer::GetStats().TriangleCount + Renderer2D::GetStats().TriangleCount;
            ss << " [meshlets]: " << Renderer::GetStats().MeshletCount;
            ss << " [2D batches]: " << Renderer2D::GetStats().BatchCount;
            ss << " [descriptor pools]: " << Renderer::GetStats().DescriptorPoolCount;
            ss << " [descriptor sets]: " << Renderer::GetStats().DescriptorSetCount;
            ss << " [samplers]: " << SamplerStorage::GetSamplerCount();
            m_Window->SetWindowTitle(ss.str().data());

            accumulatedDelta = 0.0;
            frameCount       = 0;
        }
    }
}

void Application::OnEvent(Event& e)
{
    if (m_Specification.bEnableImGui) m_UILayer->OnEvent(e);
    m_LayerQueue->OnEvent(e);

    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyButtonPressedEvent>(
        [this](const auto& event)
        {
            const auto key = event.GetKey();

            /* if (key == EKey::KEY_F1)
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
              }*/

            return false;
        });
}

Application::~Application()
{
    m_LayerQueue.reset();
    m_UILayer.reset();

    Renderer::Shutdown();
    m_Window.reset();
    m_GraphicsContext.reset();

    JobSystem::Shutdown();
    Logger::Shutdown();
    s_Instance = nullptr;
}

}  // namespace Pathfinder