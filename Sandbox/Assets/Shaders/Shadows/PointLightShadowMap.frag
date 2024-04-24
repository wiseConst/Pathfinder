#version 460

layout(location = 0) in vec4 inFragPos;

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

void main()
{
    // get distance between fragment and light source
    float lightDistance = length(inFragPos.xyz - u_Lights.PointLights[u_PC.StorageImageIndex].Position);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / u_PC.pad0.x;
    
    // write this as modified depth
    gl_FragDepth = lightDistance;
} 