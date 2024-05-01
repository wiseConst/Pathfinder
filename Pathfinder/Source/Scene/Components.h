#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Core/Core.h"

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
};

struct DirectionalLightComponent
{
    glm::vec3 Direction{0.0f};
    glm::vec3 Color{1.0f};
    float Intensity{1.0f};
    uint32_t bCastShadows{0};

    DirectionalLightComponent()                                 = default;
    DirectionalLightComponent(const DirectionalLightComponent&) = default;
    DirectionalLightComponent(const glm::vec3& direction, const glm::vec3& color, const float intensity, const uint32_t InbCastShadows)
        : Direction(direction), Color(color), Intensity(intensity), bCastShadows(InbCastShadows)
    {
    }
};

struct PointLightComponent
{
    glm::vec3 Position{0.0f};
    glm::vec3 Color{1.0f};
    float Intensity{1.0f};
    float Radius{1.0f};
    float MinRadius{0.0f};
    uint32_t bCastShadows{0};

    PointLightComponent()                           = default;
    PointLightComponent(const PointLightComponent&) = default;
    PointLightComponent(const glm::vec3& position, const glm::vec3& color, const float intensity, const float radius, const float minRadius,
                        const uint32_t InbCastShadows)
        : Position(position), Color(color), Intensity(intensity), Radius(radius), MinRadius(minRadius), bCastShadows(InbCastShadows)
    {
    }
};

}  // namespace Pathfinder

#endif