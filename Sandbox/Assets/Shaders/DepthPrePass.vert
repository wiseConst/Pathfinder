#version 460

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
} i_VertexOutput;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.glslh"

void main()
{
    gl_Position = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * u_PC.Transform * vec4(inPos, 1.0);  
    i_VertexOutput.Color = vec4(inColor.xyz * 0.2 + inNormal.xyz * 0.5, inColor.w);
    i_VertexOutput.Normal = inNormal;
    i_VertexOutput.Tangent = inTangent;
}