#include "Pathfinder.h"
#include "Core/Entrypoint.h"

#include "SandboxLayer.h"

namespace Pathfinder
{

class SandboxApp final : public Application
{
  public:
    explicit SandboxApp(const ApplicationSpecification& appSpec) : Application(appSpec) { PushLayer(MakeUnique<SandboxLayer>()); }
    ~SandboxApp() override = default;
};

Unique<Application> Create()
{
    ApplicationSpecification appSpec = {};
    appSpec.Title                    = "PATHFINDER";
    appSpec.RendererAPI              = ERendererAPI::RENDERER_API_VULKAN;
    appSpec.Width                    = 960;
    appSpec.Height                   = 640;
    appSpec.AssetsDir                = "Assets";
    appSpec.CacheDir                 = "Cached";
    appSpec.MeshDir                  = "Meshes";
    appSpec.ShadersDir               = "Shaders";
    appSpec.bEnableImGui             = true;

    return MakeUnique<SandboxApp>(appSpec);
}

}  // namespace Pathfinder
