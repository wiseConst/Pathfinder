#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(triangles, invocations = MAX_SHADOW_CASCADES) in;
layout(triangle_strip, max_vertices = 3) out;

void main()
{   
    gl_Position = u_Lights.DirLightViewProjMatrices[u_PC.StorageImageIndex * MAX_DIR_LIGHTS + gl_InvocationID] * gl_in[0].gl_Position;
    gl_Layer = gl_InvocationID;
    EmitVertex();
    
    gl_Position = u_Lights.DirLightViewProjMatrices[u_PC.StorageImageIndex * MAX_DIR_LIGHTS + gl_InvocationID] * gl_in[1].gl_Position;
    gl_Layer = gl_InvocationID;
    EmitVertex();

    gl_Position = u_Lights.DirLightViewProjMatrices[u_PC.StorageImageIndex * MAX_DIR_LIGHTS + gl_InvocationID] * gl_in[2].gl_Position;
    gl_Layer = gl_InvocationID;
    EmitVertex();
    
    EndPrimitive();
}