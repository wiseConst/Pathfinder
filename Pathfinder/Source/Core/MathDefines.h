#pragma once

#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_AVX
#define GLM_FORCE_AVX2
#define GLM_FORCE_AVX512

#define GLM_FORCE_RADIANS
#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_QUAT_DATA_XYZW
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtx/matrix_decompose.hpp>