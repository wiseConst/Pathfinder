#ifndef SCENE_H
#define SCENE_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"
#include <entt/entt.hpp>

namespace Pathfinder
{

class Entity;

class Scene final : private Uncopyable, private Unmovable
{
  public:
    Scene() = delete;
    Scene(const std::string& sceneName = s_DEFAULT_STRING);
    ~Scene() override;

    Entity CreateEntity(const std::string& entityName = s_DEFAULT_STRING);
    Entity CreateEntityWithUUID(const UUID uuid, const std::string& entityName = s_DEFAULT_STRING);
    void DestroyEntity(const Entity entity);

    void OnUpdate(const float deltaTime);

    AccelerationStructure CreateTLAS();

    NODISCARD FORCEINLINE const auto& GetName() const { return m_Name; }

  private:
    entt::registry m_Registry = {};
    std::string m_Name        = s_DEFAULT_STRING;
};

}  // namespace Pathfinder

#endif