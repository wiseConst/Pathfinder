#ifndef ENTITY_H
#define ENTITY_H

#include "Core/Core.h"
#include "Scene.h"

namespace Pathfinder
{

class Entity final
{
  public:
    Entity()  = delete;
    ~Entity() = default;

    Entity(entt::entity entity, Scene* scene);

  private:
    entt::entity m_Handle = {entt::null};
};

}  // namespace Pathfinder

#endif