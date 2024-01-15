#version 460

layout(location = 0) in vec3  inNormal;
layout(location = 1) in vec2  inUV;
layout(location = 2) in vec4  inColor;

layout(location = 0) out vec4 outFragColor;

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/ShaderDefines.h"

void main()
{
    // TODO: Remove it when u implement samplers and textures
    if(u_PC.TextureIndex != 0)
    {
         outFragColor = texture(u_GlobalTextures[nonuniformEXT(u_PC.TextureIndex)], inUV) * inColor;
    } 
    
    outFragColor = inColor;
}