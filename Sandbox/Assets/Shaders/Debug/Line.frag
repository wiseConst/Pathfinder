#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) in vec4  inColor;

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = inColor;
}