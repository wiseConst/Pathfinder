#ifndef MATH_H
#define MATH_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_CTOR_INIT
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

NODISCARD FORCEINLINE static uint32_t DivideToNextMultiple(const uint32_t dividend, const uint32_t divisor)
{
    PFR_ASSERT(divisor != 0, "Division by zero!");

    return (dividend + divisor - 1) / divisor;
}

}  // namespace Pathfinder

#endif  // MATH_H
