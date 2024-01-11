#version 460

#extension GL_GOOGLE_include_directive : require

layout (location = 0) out vec4 color;

#include "Assets/Shaders/ShaderDefines.h"


layout(set = 3, binding = 0) uniform ShaderOptions
{
    uint AlbedoTexIndex;
    uint NormalTexIndex;
    uint RoughnessTexIndex;
    uint AOTexIndex;
} u_ShaderOptions;

void main()
{
  color = vec4(1.0,0.0,0.0,1.0);
}
