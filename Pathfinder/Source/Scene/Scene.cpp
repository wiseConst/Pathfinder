#include "PathfinderPCH.h"
#include "Scene.h"

#include "Entity.h"
#include "Components.h"

namespace Pathfinder
{
Scene::Scene(const std::string& sceneName) : m_Name(sceneName) {}

Scene::~Scene()
{
    for (const auto entityIDC : m_Registry.view<IDComponent>())
        m_Registry.destroy(entityIDC);
}

void Scene::OnUpdate(const float deltaTime) {}

Entity Scene::CreateEntity(const std::string& entityName)
{
    return CreateEntityWithUUID(UUID(), entityName);
}

Entity Scene::CreateEntityWithUUID(const UUID uuid, const std::string& entityName)
{
    PFR_ASSERT(!entityName.empty(), "Entity name is empty!");

    Entity entity{m_Registry.create(), this};
    entity.AddComponent<IDComponent>(uuid);


    return entity;
}

void Scene::DestroyEntity(const Entity entity)
{
    m_Registry.destroy(entity);
}
}  // namespace Pathfinder