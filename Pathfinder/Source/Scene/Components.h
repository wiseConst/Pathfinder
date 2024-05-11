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

// Direction is stored in TransformComponent as Translation.
struct DirectionalLightComponent
{
    float Intensity       = 1.f;
    vec3 Color            = glm::vec3(1.f);
    uint32_t bCastShadows = 0;

    DirectionalLightComponent()                                 = default;
    DirectionalLightComponent(const DirectionalLightComponent&) = default;
    DirectionalLightComponent(const glm::vec3& color, const float intensity, const uint32_t InbCastShadows)
        : Color(color), Intensity(intensity), bCastShadows(InbCastShadows)
    {
    }
};

// Position is stored in TransformComponent as Translation.
struct PointLightComponent
{
    float Intensity;
    vec3 Color;
    float Radius;
    float MinRadius;
    uint32_t bCastShadows;
    bool bDrawBoundingSphere = false;

    PointLightComponent()                           = default;
    PointLightComponent(const PointLightComponent&) = default;
    PointLightComponent(const glm::vec3& color, const float intensity, const float radius, const float minRadius,
                        const uint32_t InbCastShadows)
        : Color(color), Intensity(intensity), Radius(radius), MinRadius(minRadius), bCastShadows(InbCastShadows)
    {
    }
};

// TODO: Add   bool bDrawBoundingCone = false;
// Position is stored in TransformComponent as Translation.
struct SpotLightComponent
{
    float Intensity;
    vec3 Direction;
    float Height;
    vec3 Color;
    float Radius;
    float InnerCutOff;
    float OuterCutOff;
    uint32_t bCastShadows;

    SpotLightComponent()                          = default;
    SpotLightComponent(const SpotLightComponent&) = default;
    SpotLightComponent(const glm::vec3& direction, const glm::vec3& color, const float intensity, const float height, const float radius,
                       const float innerCutOff, const float outerCutOff, const uint32_t InbCastShadows)
        : Direction(direction), Color(color), Intensity(intensity), Height(height), Radius(radius), InnerCutOff(innerCutOff),
          OuterCutOff(outerCutOff), bCastShadows(InbCastShadows)
    {
    }
};

struct MeshComponent
{
    Shared<Pathfinder::Mesh> Mesh = nullptr;
    std::string MeshSource        = s_DEFAULT_STRING;
    bool bDrawBoundingSphere      = false;

    MeshComponent()                     = default;
    MeshComponent(const MeshComponent&) = default;
    MeshComponent(const std::string& meshSource) : MeshSource(meshSource) { Mesh = Mesh::Create(MeshSource); }
};

}  // namespace Pathfinder

#endif