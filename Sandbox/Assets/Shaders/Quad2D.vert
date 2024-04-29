#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3  outNormal;
layout(location = 1) out vec2  outUV;
layout(location = 2) out vec4  outColor;

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

void main()
{
// TODO: RotateByQuat
//    const vec4 worldPos();
    gl_Position = u_PC.CameraDataBuffer.ViewProjection * u_PC.Transform * vec4(inPosition, 1.0);
    
    outUV = inUV;
    outNormal = inNormal;
    outColor = inColor;
}
