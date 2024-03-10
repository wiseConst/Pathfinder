#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out vec4 outFragColor;
layout(location = 0) in vec2 inUV;

layout(set = LAST_BINDLESS_SET + 1, binding = 0) uniform sampler2D u_Albedo;

void main()
{
    outFragColor = texture(u_Albedo, inUV);
}