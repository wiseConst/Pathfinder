#version 460

layout(location = 0) in vec3 inPos;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

void main()
{
    // NOTE: u_PC.Transform = ViewProj * Transform. Done on CPU side
    gl_Position = u_PC.Transform * vec4(inPos, 1.0);  
}