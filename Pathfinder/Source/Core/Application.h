#ifndef APPLICATION_H
#define APPLICATION_H

#include "Core.h"
#include "Renderer/RendererAPI.h"
#include "Layers/LayerQueue.h"

namespace Pathfinder
{

class Window;
class Event;
class GraphicsContext;

struct CommandLineArguments final
{
    int32_t argc = 0;
    char** argv  = nullptr;
};

struct ApplicationSpecification final
{
    ERendererAPI RendererAPI = ERendererAPI::RENDERER_API_VULKAN;
    uint32_t Width           = 1280;
    uint32_t Height          = 720;
    std::string_view Title   = "Pathfinder";
};

class Application : private Unmovable, private Uncopyable
{
  public:
    Application() = delete;
    explicit Application(const ApplicationSpecification& appSpec) noexcept;
    virtual ~Application();

    void Run();

    FORCEINLINE void PushLayer(Unique<Layer> layer)
    {
        PFR_ASSERT(m_LayerQueue, "Layer queue is not valid!");
        m_LayerQueue->Push(std::move(layer));
    }

    FORCEINLINE static void Close() { s_bIsRunning = false; }

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE const auto& GetWindow() const { return m_Window; }
    NODISCARD FORCEINLINE static const auto& Get()
    {
        PFR_ASSERT(s_Instance, "Application instance is not valid!");
        return *s_Instance;
    }

  private:
    Unique<Window> m_Window;
    Unique<GraphicsContext> m_GraphicsContext;
    Unique<LayerQueue> m_LayerQueue;

    inline static Application* s_Instance = nullptr;
    ApplicationSpecification m_Specification;
    static inline bool s_bIsRunning = false;

    void OnEvent(Event& e);
};

Unique<Application> Create(const CommandLineArguments& cmdLineArgs);

}  // namespace Pathfinder

#endif  // APPLICATION_H
