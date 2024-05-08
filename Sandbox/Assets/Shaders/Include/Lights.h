#ifdef __cplusplus
#pragma once

using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif

#define MAX_DIR_LIGHTS 4

#define LIGHT_INDEX_TYPE uint16_t
#define MAX_POINT_LIGHTS 1024
#define MAX_SPOT_LIGHTS 512

#define LIGHT_CULLING_TILE_SIZE 8u

#ifdef __cplusplus

static const LIGHT_INDEX_TYPE INVALID_LIGHT_INDEX = MAX_POINT_LIGHTS > MAX_SPOT_LIGHTS ? LIGHT_INDEX_TYPE(MAX_POINT_LIGHTS)
                                                                                       : LIGHT_INDEX_TYPE(MAX_SPOT_LIGHTS);

#else

const LIGHT_INDEX_TYPE INVALID_LIGHT_INDEX = MAX_POINT_LIGHTS > MAX_SPOT_LIGHTS ? LIGHT_INDEX_TYPE(MAX_POINT_LIGHTS)
                                                                                : LIGHT_INDEX_TYPE(MAX_SPOT_LIGHTS);

#endif

struct DirectionalLight
{
    vec3 Direction;
    float Intensity;
    vec3 Color;
    uint32_t bCastShadows;
};

struct PointLight
{
    vec3 Position;
    float Intensity;
    vec3 Color;
    float Radius;
    vec2 pad0;
    float MinRadius;
    uint32_t bCastShadows;
};

struct SpotLight
{
    vec3 Position;
    float Intensity;
    vec3 Direction;
    float Height;
    vec3 Color;
    float Radius;
    float InnerCutOff;
    float OuterCutOff;
    uint32_t bCastShadows;
    float pad0;
};

#ifndef __cplusplus
uint GetLinearGridIndex(vec2 pixelCoords, float framebufferWidth)
{
    const uint numTilesX = (uint(framebufferWidth) + LIGHT_CULLING_TILE_SIZE - 1) / LIGHT_CULLING_TILE_SIZE;
    return (uint(pixelCoords.x) / LIGHT_CULLING_TILE_SIZE) + (numTilesX * (uint(pixelCoords.y) / LIGHT_CULLING_TILE_SIZE));
}

#define PCF 1

float DirShadowCalculation(sampler2DArray dirShadowMap, const float biasMultiplier, const int cascadeIndex, const vec4 lightSpacePos,
                           const vec3 N, const vec3 L)
{
    // Perspective divison: [-w, w] -> [-1 ,1], then transform to UV space.
    const vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * .5f + .5f;
    if (projCoords.z > 1.f) return 0.f;  // Outside the far plane of the light's orthographic frustum.

    const float bias =
        max(.05f * (1.f - dot(N, L)), .005f) * biasMultiplier;  // shadow bias based on light angle && depth map resolution && slope
    float shadow = 0.f;

#if PCF
    const int halfkernelWidth = 2;
    const vec2 texelSize      = 1.f / vec2(textureSize(dirShadowMap, 0));
    for (int x = -halfkernelWidth; x <= halfkernelWidth; ++x)
    {
        for (int y = -halfkernelWidth; y <= halfkernelWidth; ++y)
        {
            const vec2 offsetUV  = projCoords.xy + vec2(float(x), float(y)) * texelSize;
            const float pcfDepth = texture(dirShadowMap, vec3(offsetUV, cascadeIndex)).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.f : 0.f;
        }
    }
    shadow /= ((halfkernelWidth * 2 + 1) * (halfkernelWidth * 2 + 1));
#else
    const float sampledDepth = texture(dirShadowMap, vec3(projCoords.xy, cascadeIndex)).r;
    shadow += projCoords.z - bias > sampledDepth ? 1.f : 0.f;
#endif

    return shadow;
}

// TODO: SSHVSM from ExileCon
#if 0 
float PointShadowCalculation(samplerCube pointShadowMap, const vec3 worldPos, const vec3 cameraPos, const PointLight pl,
                             const float farPlane)
{
#if PCF

    const vec3 sampleOffsetDirections[20] =
        vec3[](vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1), vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1),
               vec3(-1, 1, -1), vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0), vec3(1, 0, 1), vec3(-1, 0, 1),
               vec3(1, 0, -1), vec3(-1, 0, -1), vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1));

#endif

    float shadow             = 0.f;
    const vec3 lightToFrag   = worldPos - pl.Position;
    const float currentDepth = length(lightToFrag);
    const float bias         = 0.15f;

#if PCF
    const int samples        = 20;
    const float viewDistance = length(cameraPos - worldPos);
    const float diskRadius   = (1.0 + (viewDistance / farPlane)) / 25.0;
    for (int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(pointShadowMap, lightToFrag + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= farPlane;  // undo mapping [0;1]
        if (currentDepth - bias > closestDepth) shadow += 1.0;
    }
    shadow /= float(samples);
#else
    const float closestDepth = texture(pointShadowMap, lightToFrag).r * farPlane;
    shadow += currentDepth - bias > closestDepth ? 1.f : 0.f;
#endif

    return shadow;
}
#endif

#endif
