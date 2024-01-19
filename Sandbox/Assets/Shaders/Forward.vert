#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec4 outColor;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/ShaderDefines.h"

void main()
{
    gl_Position = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * vec4(inPos, 1.0);  
    outColor = vec4(inColor.xyz * 0.2 + inNormal.xyz * 0.5, inColor.w);
}