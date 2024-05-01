#ifndef SANDBOXLAYER_H
#define SANDBOXLAYER_H

#include "Pathfinder.h"

namespace Pathfinder
{

class SandboxLayer final : public Layer
{
  public:
    SandboxLayer() : Layer("SandboxLayer") {}
    ~SandboxLayer() override = default;

    void Init() final override;
    void Destroy() final override;

    void OnEvent(Event& e) final override;
    void OnUpdate(const float deltaTime) final override;
    void OnUIRender() final override;

  private:
    Shared<Camera> m_Camera = nullptr;

    Shared<Scene> m_ActiveScene = nullptr;

    std::array<PointLight, MAX_POINT_LIGHTS> m_PointLights;
    std::array<SpotLight, MAX_SPOT_LIGHTS> m_SpotLights;

    bool bRenderUI = false;
};

}  // namespace Pathfinder

#endif  // SANDBOXLAYER_H
