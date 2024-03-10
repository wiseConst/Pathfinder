#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec2 inUV;

layout(location = 0) out VertexOutput
{   
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    mat3 TBNtoWorld;
} o_VertexOutput;

void main()
{
    const vec4 WorldPos = u_PC.Transform * vec4(inPos, 1.0);
    gl_Position = u_GlobalCameraData.ViewProjection * WorldPos;
    
    o_VertexOutput.Color = inColor;
    o_VertexOutput.UV = inUV;
    o_VertexOutput.WorldPos = WorldPos.xyz;
    
    const mat3 normalMatrix = transpose(inverse(mat3(u_PC.Transform))); // Better to calculate it on CPU
    const vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(normalMatrix * inTangent);
    T = normalize(T - dot(T, N) * N); // Reorthogonalization around N via Gramm-Schmidt.
    const vec3 B = cross(N, T);
    o_VertexOutput.TBNtoWorld = mat3(T, B, N);
}