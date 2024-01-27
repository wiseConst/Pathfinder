#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
    vec2 UV;
} i_VertexInput;

void main()
{
    outFragColor = vec4(texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], i_VertexInput.UV).xyz, 1.0);// * i_VertexInput.Color;
}