#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inUV;

layout(location = 0) out VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
    vec2 UV;
} o_VertexOutput;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

void main()
{
    gl_Position = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * u_PC.Transform * vec4(inPos, 1.0);  
    o_VertexOutput.Color = inColor;
    o_VertexOutput.Normal = inNormal;
    o_VertexOutput.Tangent = inTangent;
    o_VertexOutput.UV = inUV;
}