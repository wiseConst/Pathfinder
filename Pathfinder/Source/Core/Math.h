#ifndef MATH_H
#define MATH_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_QUAT_DATA_XYZW
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>

#include "CoreDefines.h"

namespace Pathfinder
{

constexpr const auto s_PFR_SMALL_NUMBER = 10.E-9f;

NODISCARD FORCEINLINE bool IsNearlyZero(const double val)
{
    return glm::abs(val) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE bool IsNearlyZero(const float val)
{
    return glm::abs(val) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE bool IsNearlyEqual(const double lhs, const double rhs)
{
    return glm::abs(rhs - lhs) <= s_PFR_SMALL_NUMBER;
}

NODISCARD FORCEINLINE bool IsNearlyEqual(const float lhs, const float rhs)
{
    return glm::abs(rhs - lhs) <= s_PFR_SMALL_NUMBER;
}

}  // namespace Pathfinder

#endif  // MATH_H
