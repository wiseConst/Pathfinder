#ifdef __cplusplus
#pragma once

using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif

#define MAX_POINT_LIGHTS 60
#define MAX_SPOT_LIGHTS 20
#define MAX_DIR_LIGHTS 8

struct PointLight
{
    vec3 Position;
    vec3 Color;
    float Intensity;
    float Radius;
    float MinRadius;
    uint32_t bCastShadows;
};

struct DirectionalLight
{
    vec3 Direction;
    vec3 Color;
    float Intensity;
    uint32_t bCastShadows;
};

struct SpotLight
{
    vec3 Position;
    vec3 SpotDirection;
    vec3 Color;
    float Intensity;
    float InnerCutOff;
    float OuterCutOff;
};

// TODO: Fill SpotLight