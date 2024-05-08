#include "PathfinderPCH.h"
#include "Entity.h"

namespace Pathfinder
{

Entity::Entity(entt::entity entity, Scene* scenePtr) : m_Handle(entity), m_ScenePtr(scenePtr) {}

}  // namespace Pathfinder