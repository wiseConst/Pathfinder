#ifndef ENTITY_H
#define ENTITY_H

#include "Core/Core.h"
#include "Scene.h"
#include "Components.h"

namespace Pathfinder
{

class Entity final
{
  public:
    Entity(entt::entity entity, Scene* scenePtr);
    ~Entity() = default;

    operator entt::entity() const { return m_Handle; }

    template <typename T> NODISCARD FORCEINLINE bool HasComponent() const
    {
        PFR_ASSERT(IsValid(), "Entity or its scene ptr is not valid!");
        return m_ScenePtr->m_Registry.all_of<T>(m_Handle);
    }

    template <typename T, typename... Args> FORCEINLINE T& AddComponent(Args&&... args)
    {
        PFR_ASSERT(IsValid(), "Entity or its scene ptr is not valid!");
        PFR_ASSERT(!HasComponent<T>(), "Entity already has the component!");

        return m_ScenePtr->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
    }

    template <typename T> NODISCARD FORCEINLINE T& GetComponent() const
    {
        PFR_ASSERT(IsValid(), "Entity or its scene ptr is not valid!");
        PFR_ASSERT(HasComponent<T>(), "Entity doesn't have the component!");

        return m_ScenePtr->m_Registry.get<T>(m_Handle);
    }

    template <typename T> FORCEINLINE void RemoveComponent()
    {
        PFR_ASSERT(IsValid(), "Entity or its scene ptr is not valid!");
        PFR_ASSERT(HasComponent<T>(), "Attempting to delete component that entity doesn't have.");

        if (typeid(T) == typeid(IDComponent) || typeid(T) == typeid(TransformComponent) || typeid(T) == typeid(TagComponent))
        {
            LOG_WARN("Attempting to delete Transform/Tag/ID component! Returning...");
            return;
        }

        m_ScenePtr->m_Registry.remove<T>(m_Handle);
    }

    NODISCARD FORCEINLINE bool IsValid() const { return m_ScenePtr && m_Handle != entt::null; }
    NODISCARD FORCEINLINE bool operator==(const Entity& that) const { return m_Handle == that.m_Handle && m_ScenePtr == that.m_ScenePtr; }
    NODISCARD FORCEINLINE bool operator!=(const Entity& that) const { return !(*this == that); }
    NODISCARD FORCEINLINE const auto& GetUUID() const { return GetComponent<IDComponent>().ID; }

  private:
    entt::entity m_Handle = {entt::null};
    Scene* m_ScenePtr     = nullptr;

    Entity() = delete;
};

}  // namespace Pathfinder

#endif