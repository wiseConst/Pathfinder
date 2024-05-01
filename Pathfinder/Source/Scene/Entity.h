#ifndef ENTITY_H
#define ENTITY_H

#include "Core/Core.h"
#include "Scene.h"

namespace Pathfinder
{

class Entity final
{
  public:
    Entity(entt::entity entity, Scene* scenePtr);
    ~Entity() = default;

    operator entt::entity() const { return m_Handle; }

  private:
    entt::entity m_Handle = {entt::null};
    Scene* m_ScenePtr     = nullptr;

    Entity() = delete;
};

}  // namespace Pathfinder

#endif