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
};

#ifndef __cplusplus

uint GetLinearGridIndex(vec2 pixelCoords, float framebufferWidth)
{
    const uint numTilesX = (uint(framebufferWidth) + LIGHT_CULLING_TILE_SIZE - 1) / LIGHT_CULLING_TILE_SIZE;
    return (uint(pixelCoords.x) / LIGHT_CULLING_TILE_SIZE) + (numTilesX * (uint(pixelCoords.y) / LIGHT_CULLING_TILE_SIZE));
}

#endif
