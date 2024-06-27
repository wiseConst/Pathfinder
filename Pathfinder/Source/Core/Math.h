#pragma once

#include "MathDefines.h"
#include "CoreDefines.h"
#include "Primitives.h"

namespace Pathfinder
{

static constexpr auto s_PFR_SMALL_NUMBER       = 10.E-8f;
static constexpr auto s_PFR_KINDA_SMALL_NUMBER = 10.E-4f;

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

NODISCARD FORCEINLINE static bool IsNearlyEqual(const glm::vec3& lhs, const glm::vec3& rhs, const float Tolerance = s_PFR_SMALL_NUMBER)
{
    return glm::abs(rhs.x - lhs.x) <= Tolerance && glm::abs(rhs.y - lhs.y) <= Tolerance && glm::abs(rhs.z - lhs.z) <= Tolerance;
}

NODISCARD FORCEINLINE static Plane ComputePlane(const glm::vec3& p0, const glm::vec3& normal)
{
    Plane plane    = {.Normal = glm::normalize(normal), .Distance = 0};
    plane.Distance = glm::dot(p0, plane.Normal);  // signed distance to the origin using p0

    return plane;
}

}  // namespace Pathfinder
