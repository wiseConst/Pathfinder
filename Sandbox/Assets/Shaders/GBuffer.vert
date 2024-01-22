#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outColor;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

void main()
{
    gl_Position = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * vec4(inPos, 1.0);
    
    
    outPosition = vec4(inPos, 1.0);
    outNormal = vec4(inNormal, 1.0);
    outColor = inColor;
}