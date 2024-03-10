#ifdef __cplusplus
#pragma once

using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif

#define MAX_POINT_LIGHTS 1024
#define MAX_SPOT_LIGHTS 1024
#define MAX_DIR_LIGHTS 4

// NOTE: Strange as fuck, but 32*32 gives way-way more perf than 16*16 on RTX 3050 TI LAPTOP

#ifdef __cplusplus

static const uint32_t LIGHT_CULLING_TILE_SIZE = 16u;
static const uint32_t INVALID_LIGHT_INDEX     = MAX_POINT_LIGHTS > MAX_SPOT_LIGHTS ? MAX_POINT_LIGHTS : MAX_SPOT_LIGHTS;

#else

const uint32_t LIGHT_CULLING_TILE_SIZE = 16u;
const uint32_t INVALID_LIGHT_INDEX     = MAX_POINT_LIGHTS > MAX_SPOT_LIGHTS ? MAX_POINT_LIGHTS : MAX_SPOT_LIGHTS;

#endif

#ifndef __cplusplus
uint GetLinearGridIndex(vec2 pixelCoords, float framebufferWidth)
{
    const uint numTilesX = (uint(framebufferWidth) + LIGHT_CULLING_TILE_SIZE - 1) / LIGHT_CULLING_TILE_SIZE;
    return (uint(pixelCoords.x) / LIGHT_CULLING_TILE_SIZE) + (numTilesX * (uint(pixelCoords.y) / LIGHT_CULLING_TILE_SIZE));
}

float ShadowCalculation(sampler2D dirShadowMap, const vec4 lightSpacePos, const vec3 N, const vec3 L)
{
    // Perspective divison: [-w, w] -> [-1 ,1], then transform to UV space.
    const vec3 projCoords = (lightSpacePos.xyz / lightSpacePos.w) * .5f + .5f;
    if (projCoords.z > 1.f) return 0.f;  // Outside the far plane of the light's orthographic frustum.

    const float bias = max(.05f * (1.f - dot(N, L)), .005f);  // shadow bias based on light angle
    float shadow     = 0.f;

    const int halfkernelWidth = 3;
    const vec2 texelSize      = 1.f / textureSize(dirShadowMap, 0);
    for (int x = -halfkernelWidth; x <= halfkernelWidth; ++x)
    {
        for (int y = -halfkernelWidth; y <= halfkernelWidth; ++y)
        {
            const float pcfDepth = texture(dirShadowMap, projCoords.xy + vec2(float(x), float(y)) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.f : 0.f;
        }
    }
    shadow /= ((halfkernelWidth * 2 + 1) * (halfkernelWidth * 2 + 1));

    return shadow;
}

#endif

struct PointLight
{
    vec3 Position;
    vec3 Color;
    float Intensity;
    float Radius;
    float MinRadius;
    bool bCastShadows;
};

struct DirectionalLight
{
    vec3 Direction;
    vec3 Color;
    float Intensity;
    bool bCastShadows;
};

struct SpotLight
{
    vec3 Position;
    vec3 Direction;
    vec3 Color;
    float Intensity;
    float Height;
    float Radius;
    float InnerCutOff;
    float OuterCutOff;
};