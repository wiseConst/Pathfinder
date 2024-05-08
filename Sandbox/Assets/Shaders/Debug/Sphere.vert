#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

 struct DebugSphereRenderInfo
 {
    vec3 Translation;
    vec4 Orientation;
    vec3 Scale;
    vec3 Center;
    float Radius;
    vec4 Color;
 };

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer DebugSphereRenderInfoBuffer
{
    DebugSphereRenderInfo sphereInfos[];
}
s_DebugSphereRenderInfoBufferBDA;  // Name unused, check u_PC

void main()
{
    DebugSphereRenderInfoBuffer infoBuffer = DebugSphereRenderInfoBuffer(u_PC.LightDataBuffer);
    DebugSphereRenderInfo sphereInfo =  infoBuffer.sphereInfos[u_PC.StorageImageIndex];

    const vec3 worldPos = RotateByQuat((inPosition * sphereInfo.Radius + sphereInfo.Center) * sphereInfo.Scale, sphereInfo.Orientation) + sphereInfo.Translation;
    gl_Position = u_PC.CameraDataBuffer.ViewProjection * vec4(worldPos, 1.0);
    
    outColor = sphereInfo.Color;
}
