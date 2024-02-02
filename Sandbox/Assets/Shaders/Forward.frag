#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"
#include "Assets/Shaders/Include/PhongShading.glslh"

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    mat3 TBNtoWorld;
} i_VertexInput;

void main()
{
    const vec3 normalMap = texture(u_GlobalTextures[nonuniformEXT(u_PC.NormalTextureIndex)], i_VertexInput.UV).xyz  * 2.0 - 1.0;
    const vec3 N = normalize(i_VertexInput.TBNtoWorld * normalMap);
    
    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], i_VertexInput.UV);
    const vec3 lightColor = vec3(0.5, 0.3, 0.1);
    const vec3 lightDir = vec3(3, -5, -11);
  
    const vec4 dirLighting = ApplyPhongShading(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position , lightColor, lightDir);
    outFragColor = dirLighting * albedo * i_VertexInput.Color;
}