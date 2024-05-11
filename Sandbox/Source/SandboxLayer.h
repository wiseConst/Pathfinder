#ifndef SANDBOXLAYER_H
#define SANDBOXLAYER_H

#include "Pathfinder.h"

namespace Pathfinder
{

class SceneHierarchyPanel;

class SandboxLayer final : public Layer
{
  public:
    SandboxLayer();
    ~SandboxLayer();

    void Init() final override;
    void Destroy() final override;

    void OnEvent(Event& e) final override;
    void OnUpdate(const float deltaTime) final override;
    void OnUIRender() final override;

  private:
    Shared<Camera> m_Camera                          = nullptr;
    Shared<Scene> m_ActiveScene                      = nullptr;
    Unique<SceneHierarchyPanel> m_WorldOutlinerPanel = nullptr;

    bool bRenderUI = false;
};

}  // namespace Pathfinder

#endif  // SANDBOXLAYER_H
