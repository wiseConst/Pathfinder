#ifndef CULLING_H
#define CULLING_H

#ifdef __cplusplus
#include "Primitives.h"
#else 
#include "Include/Primitives.h"
#endif

Plane ComputePlane(const vec3 p0, const vec3 p1, const vec3 p2)
{
    Plane plane;

    plane.Normal   = normalize(cross(p1 - p0, p2 - p0));
    plane.Distance = dot(plane.Normal, p0);  // signed distance to the origin using p0

    return plane;
}

// NOTE: Great explanation of why it is like that: https://www.youtube.com/watch?v=4p-E_31XOPM
// Check to see if a sphere is fully behind (inside the negative halfspace of) a plane. (behind normal of the plane)
// Source: Real-time collision detection, Christer Ericson (2005)
bool SphereInsidePlane(Sphere sphere, Plane plane)
{
    return dot(plane.Normal, sphere.Center) - plane.Distance + sphere.Radius <= 0.0;
}

bool SphereInsideTileFrustum(Sphere sphere, TileFrustum frustum, float zNear, float zFar)
{
    // Better to just unroll:
    bool result = true;

    // First check depth(behind the far plane of in front of near)
    result = ((sphere.Center.z - sphere.Radius > zNear || sphere.Center.z + sphere.Radius < zFar) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[0]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[1]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[2]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[3]) ? false : result);

    return result;
}

bool SphereIntersectsAABB(Sphere sphere, AABB aabb)
{
    vec3 vDelta   = max(vec3(0.0), abs(aabb.Center - sphere.Center) - aabb.Extents);
    float fDistSq = dot(vDelta, vDelta);
    return fDistSq <= sphere.Radius * sphere.Radius;
}

// Check to see if a point is fully behind (inside the negative halfspace of) a plane.
bool PointInsidePlane(vec3 p, Plane plane)
{
    return dot(plane.Normal, p) - plane.Distance < 0;
}

bool ConeInsidePlane(Cone cone, Plane plane)
{
    // Compute the farthest point on the end of the cone to the positive space of the plane.
    const vec3 m = cross(cross(plane.Normal, cone.Direction), cone.Direction);
    const vec3 Q = cone.Tip + cone.Height * cone.Direction - cone.Radius * m;

    // The cone is in the negative halfspace of the plane if both
    // the tip of the cone and the farthest point on the end of the cone to the
    // positive halfspace of the plane are both inside the negative halfspace
    // of the plane.
    return PointInsidePlane(cone.Tip, plane) && PointInsidePlane(Q, plane);
}

bool ConeInsideTileFrustum(Cone cone, TileFrustum frustum, float zNear, float zFar)
{
    // NOTE: View space, facing towards -Z
    const Plane nearPlane = {vec3(0, 0, -1), -zNear};
    const Plane farPlane  = {vec3(0, 0, 1), zFar};

    if (ConeInsidePlane(cone, nearPlane) || ConeInsidePlane(cone, farPlane)) return false;

    for (uint i = 0; i < 4; ++i)
    {
        if (ConeInsidePlane(cone, frustum.Planes[i])) return false;
    }

    return true;
}

bool SphereInsideFrustum(Sphere sphere, Frustum frustum)
{
    // Better to just unroll:
    bool result = true;

    result = (SphereInsidePlane(sphere, frustum.Planes[0]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[1]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[2]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[3]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[4]) ? false : result);
    result = (SphereInsidePlane(sphere, frustum.Planes[5]) ? false : result);

    return result;
}

#endif