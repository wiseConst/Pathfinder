#pragma once

#include "Core/Core.h"
#include "Scene/Entity.h"

namespace Pathfinder
{

class Scene;

class SceneHierarchyPanel final : private Uncopyable, private Unmovable
{
  public:
    SceneHierarchyPanel() = default;
    SceneHierarchyPanel(const Shared<Scene>& context);
    ~SceneHierarchyPanel() override = default;

    void SetContext(const Shared<Scene>& context);

    void OnImGuiRender();

  private:
    Shared<Scene> m_Context;
    Entity m_SelectionContext;

    void DrawEntityNode(Entity entity);
    void ShowComponents(Entity entity);
};

}  // namespace Pathfinder