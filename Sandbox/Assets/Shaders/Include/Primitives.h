#ifndef PRIMITIVES_H
#define PRIMITIVES_H

struct Plane
{
    vec3 Normal;  // Should be normalized.
    // LearnOpenGL:  distance from origin to the nearest point in the plane(PERPENDICULAR VECTOR FROM THE ORIGIN IN OTHER WORDS)
    float Distance;  // Signed distance from the plane to the origin of the world(or whatever coordinate space you're working in).
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