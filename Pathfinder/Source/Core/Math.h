#pragma once

#include "MathDefines.h"
#include "CoreDefines.h"
#include "Primitives.h"

namespace Pathfinder
{

// TODO: Replace with glm type packing.

static constexpr auto s_PFR_SMALL_NUMBER = 10.E-9f;

NODISCARD FORCEINLINE static bool IsNearlyZero(const double val)
{
    return glm::abs(val) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE static bool IsNearlyZero(const float val)
{
    return glm::abs(val) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE static bool IsNearlyEqual(const double lhs, const double rhs)
{
    return glm::abs(rhs - lhs) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE static bool IsNearlyEqual(const float lhs, const float rhs)
{
    return glm::abs(rhs - lhs) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE static Plane ComputePlane(const glm::vec3& p0, const glm::vec3& normal)
{
    Plane plane    = {glm::normalize(normal), 0};
    plane.Distance = glm::dot(p0, plane.Normal);  // signed distance to the origin using p0

    return plane;
}

}  // namespace Pathfinder
