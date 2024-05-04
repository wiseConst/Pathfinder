#ifndef MATH_H
#define MATH_H

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_AVX
#define GLM_FORCE_AVX2
#define GLM_FORCE_AVX512

#define GLM_FORCE_RADIANS
#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_QUAT_DATA_XYZW
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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

static glm::u8vec4 PackVec4ToU8Vec4(const glm::vec4& vec)
{
    // Normalize the components to [0, 255]
    const uint8_t r = static_cast<uint8_t>(vec.r * 255.0f);
    const uint8_t g = static_cast<uint8_t>(vec.g * 255.0f);
    const uint8_t b = static_cast<uint8_t>(vec.b * 255.0f);
    const uint8_t a = static_cast<uint8_t>(vec.a * 255.0f);

    return glm::u8vec4(r, g, b, a);
}

}  // namespace Pathfinder

#endif  // MATH_H
