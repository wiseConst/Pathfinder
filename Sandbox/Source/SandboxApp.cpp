#include "Pathfinder.h"
#include "Core/Entrypoint.h"

namespace Pathfinder
{

class SandboxApp final : public Application
{
  public:
    explicit SandboxApp(const ApplicationSpecification& appSpec) : Application(appSpec) {}
    ~SandboxApp() override = default;
};

Unique<Application> Create(const CommandLineArguments& cmdLineArgs)
{
    ApplicationSpecification appSpec = {};
    appSpec.RendererAPI              = ERendererAPI::RENDERER_API_VULKAN;
    appSpec.Title                    = "RTX, mesh-shading enjoyer";

    return MakeUnique<SandboxApp>(appSpec);
}

}  // namespace Pathfinder
