#ifdef __cplusplus
#pragma once

using vec3 = glm::vec3;
using vec4 = glm::vec4;

#endif 

struct PointLight
{
    vec3 Position;
    vec3 Color;
    float Intensity;
};

struct DirectionalLight
{
    vec3 Direction;
    vec3 Color;
    float Intensity;
};

// TODO: Fill SpotLight