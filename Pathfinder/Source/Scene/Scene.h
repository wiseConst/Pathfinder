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

    // NOTE: Can pass lambda, but first param should be entityID(entt::entity), and next params are references for Components.
    // Example: scene.ForEach<Position, Velocity, Color>([](const auto entity, Position& pos, Velocity& vel, Color& col) {}
    template <typename... Components, typename Func> void ForEach(Func&& func) { m_Registry.view<Components...>().each(func); }

    void OnUpdate(const float deltaTime);

    const auto& GetTLAS()
    {
        if (!m_TLAS.Buffer || !m_TLAS.Handle) RebuildTLAS();
        return m_TLAS;
    }

    NODISCARD FORCEINLINE const auto& GetName() const { return m_Name; }
    NODISCARD FORCEINLINE const auto GetEntityCount() const { return m_EntityCount; }

  private:
    entt::registry m_Registry = {};
    std::string m_Name        = s_DEFAULT_STRING;
    uint32_t m_EntityCount    = 0;

    AccelerationStructure m_TLAS = {};

    void RebuildTLAS();

    friend class Entity;
    friend class SceneManager;
};

}  // namespace Pathfinder

#endif