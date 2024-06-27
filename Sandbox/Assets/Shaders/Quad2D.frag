#version 460 core

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) in vec2      inUV;
layout(location = 1) in vec4      inColor;
layout(location = 2) in flat uint inTextureIndex; 

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(u_GlobalTextures[nonuniformEXT(inTextureIndex)], inUV) * inColor;
}