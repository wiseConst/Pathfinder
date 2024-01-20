#version 460

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.glslh"


void main()
{
    outPosition = inPosition;
    outNormal = inNormal;
    outAlbedo = inColor;
}