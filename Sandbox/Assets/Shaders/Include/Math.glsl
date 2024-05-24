//#pragma once

uint32_t DivideToNextMultiple(const uint32_t dividend, const uint32_t divisor)
{
    if(divisor == 0) return 0;

    return (dividend + divisor - 1) / divisor;
}

mat3 QuatToRotMat3(const vec4 q) {
    const vec3 q2 = q.xyz * q.xyz;
    const vec3 qq2 = q.xyz * q2;
    const vec3 qq1 = q.xyz * q.w;

    return mat3(
        1.0 - 2.0 * (q2.y + q2.z), 2.0 * (qq2.x + qq1.z),     2.0 * (qq2.y - qq1.y),
        2.0 * (qq2.x - qq1.z),     1.0 - 2.0 * (q2.x + q2.z), 2.0 * (qq2.z + qq1.x),
        2.0 * (qq2.y + qq1.y),     2.0 * (qq2.z - qq1.x),     1.0 - 2.0 * (q2.x + q2.y)
    );
}

// https://developer.download.nvidia.com/cg/saturate.html
float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec4 UnpackU8Vec4ToVec4(u8vec4 packed)
{
    const float invDivisor = 1 / 255.0f;
    const float r          = invDivisor * packed.r;
    const float g          = invDivisor * packed.g;
    const float b          = invDivisor * packed.b;
    const float a          = invDivisor * packed.a;

    return vec4(r, g, b, a);
}

// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
vec3 RotateByQuat(const vec3 position, const vec4 orientation)
{
    const vec3 uv = 2.0f * cross(orientation.xyz, position);

    // NOTE: That the center of rotation is always the origin, so we adding an offset(position).
    return position + uv * orientation.w + cross(orientation.xyz, uv);
}