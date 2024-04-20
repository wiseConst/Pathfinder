#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

void main()
{
    gl_Position = u_PC.Transform * vec4(s_GlobalVertexPosBuffers[u_PC.VertexPosBufferIndex].Positions[gl_VertexIndex].Position, 1.0);
}