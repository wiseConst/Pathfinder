#pragma once

#include "Core.h"
#include <Renderer/RendererAPI.h>
#include <Layers/LayerQueue.h>
#include <Layers/UILayer.h>

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
    std::string WorkingDir           = s_DEFAULT_STRING;
    std::string AssetsDir            = s_DEFAULT_STRING;
    std::string MeshDir              = s_DEFAULT_STRING;
    std::string ShadersDir           = s_DEFAULT_STRING;
    std::string CacheDir             = s_DEFAULT_STRING;
    std::string_view Title           = "Pathfinder";
    CommandLineArguments CmdLineArgs = {};
    uint32_t Width                   = 1280;
    uint32_t Height                  = 720;
    ERendererAPI RendererAPI         = ERendererAPI::RENDERER_API_VULKAN;
    bool bEnableImGui                = false;
};

class Application : private Unmovable, private Uncopyable
{
  public:
    explicit Application(const ApplicationSpecification& appSpec) noexcept;
    virtual ~Application();

    FORCEINLINE void SetCommandLineArguments(const CommandLineArguments& cmdLineArgs) { m_Specification.CmdLineArgs = cmdLineArgs; }
    void Run();

    FORCEINLINE void PushLayer(Unique<Layer> layer)
    {
        PFR_ASSERT(m_LayerQueue, "Layer queue is not valid!");
        m_LayerQueue->Push(std::move(layer));
    }

    FORCEINLINE static void Close() { s_bIsRunning = false; }

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE const auto& GetWindow() const { return m_Window; }
    //  NODISCARD FORCEINLINE const auto& GetUILayer() const { return m_UILayer; }
    NODISCARD FORCEINLINE static const auto& Get()
    {
        PFR_ASSERT(s_Instance, "Application instance is not valid!");
        return *s_Instance;
    }

    NODISCARD FORCEINLINE const auto GetCurrentFrameNumber() const { return m_FrameNumber; }

  private:
    static inline Application* s_Instance = nullptr;
    ApplicationSpecification m_Specification;

    Unique<Window> m_Window;
    Unique<GraphicsContext> m_GraphicsContext;
    Unique<LayerQueue> m_LayerQueue;
    Unique<UILayer> m_UILayer;

    float m_DeltaTime               = 0.0f;
    static inline bool s_bIsRunning = false;
    uint32_t m_FrameNumber{};

    void OnEvent(Event& e);
    Application() = delete;
};

Unique<Application> Create();

}  // namespace Pathfinder
