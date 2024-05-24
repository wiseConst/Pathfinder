#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

struct DebugSphereData
{
    vec3 Translation;
    vec3 Scale;
    vec4 Orientation;
    vec3 Center;
    float Radius;
    u8vec4 Color;
};

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer DebugSphereDataBuffer
{
    DebugSphereData debugSpheres[];
}
s_DebugSphereDataBufferBDA;  // Name unused, check u_PC

void main()
{
    const DebugSphereData sphereData = DebugSphereDataBuffer(u_PC.addr0).debugSpheres[gl_InstanceIndex];

    const vec3 worldPos = RotateByQuat((inPosition * sphereData.Radius + sphereData.Center) * sphereData.Scale, sphereData.Orientation) + sphereData.Translation;
    gl_Position = CameraData(u_PC.CameraDataBuffer).ViewProjection * vec4(worldPos, 1.0);
    
    outColor = UnpackU8Vec4ToVec4(sphereData.Color);
}
