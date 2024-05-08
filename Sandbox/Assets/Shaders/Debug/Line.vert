#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    gl_Position = u_PC.CameraDataBuffer.ViewProjection * vec4(inPosition, 1.0);
    
    outColor = inColor;
}
