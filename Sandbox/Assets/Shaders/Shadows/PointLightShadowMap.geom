#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout(location = 0) out vec4 outFragPos; // FragPos from GS (output per emitvertex)

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

/*
    If a shader statically assigns a value to gl_Layer, layered rendering mode is enabled.
    ...
    If the geometry stage makes no static assignment to gl_Layer, the input gl_Layer in the fragment stage will be zero.
*/

void EmitFace(const mat4 viewProjectionMatrix)
{
    for(int i = 0; i < 3; ++i)
    {
        outFragPos = gl_in[i].gl_Position;
        gl_Position = viewProjectionMatrix * outFragPos;
        EmitVertex();
    }
    EndPrimitive();
}

void main()
{
  gl_Layer = 0;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 0]);
  
  gl_Layer = 1;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 1]);
  
  gl_Layer = 2;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 2]);
  
  gl_Layer = 3;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 3]);
  
  gl_Layer = 4;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 4]);
  
  gl_Layer = 5;
  EmitFace(u_Lights.PointLightViewProjMatrices[u_PC.StorageImageIndex * 6 + 5]);
}