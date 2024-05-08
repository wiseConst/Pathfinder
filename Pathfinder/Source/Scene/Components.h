#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Core/Core.h"
#include "Renderer/Mesh/Mesh.h"
#include "Lights.h"

namespace Pathfinder
{

// TODO: Maybe add camera component to switch between them?? or for multiple viewports??

struct IDComponent
{
    UUID ID;

    IDComponent()                   = default;
    IDComponent(const IDComponent&) = default;
    IDComponent(const UUID& uuid) : ID(uuid) {}
};

struct TagComponent
{
    std::string Tag{s_DEFAULT_STRING};

    TagComponent()                    = default;
    TagComponent(const TagComponent&) = default;
    TagComponent(const std::string& tag) : Tag(tag) {}
};

// FIXME: On cpu im using matrices, but on gpu quaternions, move to quaternions everywhere.
struct TransformComponent
{
    glm::vec3 Translation{0.0f};
    glm::vec3 Rotation{0.0f};
    glm::vec3 Scale{1.0f};

    TransformComponent()                          = default;
    TransformComponent(const TransformComponent&) = default;
    TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
        : Translation(translation), Rotation(rotation), Scale(scale)
    {
    }

    glm::mat4 GetTransform() const
    {
        const glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), glm::vec3(1, 0, 0)) *
                                         glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), glm::vec3(0, 1, 0)) *
                                         glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), glm::vec3(0, 0, 1));

        return glm::translate(glm::mat4(1.0f), Translation) * rotationMatrix * glm::scale(glm::mat4(1.0f), Scale);
    }

    operator glm::mat4() const { return GetTransform(); }
};

struct DirectionalLightComponent
{
    DirectionalLight dl = {};

    operator DirectionalLight() const { return dl; }

    DirectionalLightComponent()                                 = default;
    DirectionalLightComponent(const DirectionalLightComponent&) = default;
    DirectionalLightComponent(const DirectionalLight& that) { dl = that; }
    DirectionalLightComponent(const glm::vec3& direction, const glm::vec3& color, const float intensity, const uint32_t bCastShadows)
        : dl{direction, intensity, color, bCastShadows}
    {
    }
};

struct PointLightComponent
{
    PointLight pl = {};

    operator PointLight() const { return pl; }

    PointLightComponent()                           = default;
    PointLightComponent(const PointLightComponent&) = default;
    PointLightComponent(const PointLight& that) { pl = that; }
    PointLightComponent(const glm::vec3& position, const glm::vec3& color, const float intensity, const float radius, const float minRadius,
                        const uint32_t bCastShadows)
        : pl{position, intensity, color, radius, glm::vec2{0, 0}, minRadius, bCastShadows}
    {
    }
};

struct SpotLightComponent
{
    SpotLight sl = {};

    operator SpotLight() const { return sl; }

    SpotLightComponent()                          = default;
    SpotLightComponent(const SpotLightComponent&) = default;
    SpotLightComponent(const SpotLight& that) { sl = that; }
    SpotLightComponent(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, const float intensity,
                       const float height, const float radius, const float innerCutOff, const float outerCutOff,
                       const uint32_t bCastShadows)
        : sl{position, intensity, direction, height, color, radius, innerCutOff, outerCutOff, bCastShadows, 0.0f}
    {
    }
};

struct MeshComponent
{
    Shared<Pathfinder::Mesh> Mesh = nullptr;
    std::string MeshSource        = s_DEFAULT_STRING;

    MeshComponent()                     = default;
    MeshComponent(const MeshComponent&) = default;
    MeshComponent(const std::string& meshSource) : MeshSource(meshSource) { Mesh = Mesh::Create(MeshSource); }
};

}  // namespace Pathfinder

#endif