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

Unique<Application> Create()
{
    ApplicationSpecification appSpec = {};
    appSpec.RendererAPI              = ERendererAPI::RENDERER_API_VULKAN;
    appSpec.Title                    = "PATHFINDER";
    appSpec.Width                    = 960;
    appSpec.Height                   = 640;

    return MakeUnique<SandboxApp>(appSpec);
}

}  // namespace Pathfinder
