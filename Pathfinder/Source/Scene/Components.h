#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Core/Core.h"

namespace Pathfinder
{

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
    bool Padlock{false};

    TransformComponent()                          = default;
    TransformComponent(const TransformComponent&) = default;
    TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
        : Translation(translation), Rotation(rotation), Scale(scale)
    {
    }
};

}  // namespace Pathfinder

#endif