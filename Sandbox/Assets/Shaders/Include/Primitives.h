#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#ifdef __cplusplus
using vec2    = glm::vec2;
using u16vec2 = glm::u16vec2;
using u8vec4  = glm::u8vec4;
using vec3    = glm::vec3;
using vec4    = glm::vec4;
using mat4    = glm::mat4;
#endif

struct Plane
{
    vec3 Normal;
    float Distance;
};

struct AABB
{
    vec3 Center;
    vec3 Extents;
};

// Left, Right, Top, Bottom, Near, Far
struct Frustum
{
    Plane Planes[6];
};

// Used for light culling
// Left, Right, Top, Bottom
struct TileFrustum
{
    Plane Planes[4];
};

struct Sphere
{
    vec3 Center;
    float Radius;
};

struct Cone
{
    vec3 Tip;
    float Height;
    vec3 Direction;
    float Radius;
};

#endif