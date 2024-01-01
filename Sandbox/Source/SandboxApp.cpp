#include "Pathfinder.h"
#include "Core/Entrypoint.h"

#include "SandboxLayer.h"

namespace Pathfinder
{

class SandboxApp final : public Application
{
  public:
    explicit SandboxApp(const ApplicationSpecification& appSpec) : Application(appSpec)
    {
        PushLayer(MakeUnique<SandboxLayer>());
    }
    ~SandboxApp() override = default;
};

Unique<Application> Create(const CommandLineArguments& cmdLineArgs)
{
    ApplicationSpecification appSpec = {};
    appSpec.RendererAPI              = ERendererAPI::RENDERER_API_VULKAN;
    appSpec.Title                    = "RTX, mesh-shading enjoyer";
    appSpec.Width                    = 720;
    appSpec.Height                   = 540;

    return MakeUnique<SandboxApp>(appSpec);
}

}  // namespace Pathfinder
