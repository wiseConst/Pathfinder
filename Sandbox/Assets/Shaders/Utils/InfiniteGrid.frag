#version 460

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 inNearPoint;
layout(location = 1) in vec3 inFarPoint;    

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

// https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

vec4 Gridify(vec3 fragPos3D, float scale)
{
   const vec2 coord = fragPos3D.xz * scale;
   const vec2 derivative = fwidth(coord);
   const vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
   const float line = min(grid.x, grid.y);
   const float minX = min(derivative.x, 1);
   const float minZ = min(derivative.y, 1);
   vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
   
   // Z axis
   if(fragPos3D.x > -0.1 * minX && fragPos3D.x < 0.1 * minX)
      color.z = 1.0;

   // X axis
   if(fragPos3D.z > -0.1 * minZ && fragPos3D.z < 0.1 * minZ)
      color.x = 1.0;

   return color;
}

float ComputeDepth(vec3 fragPos3D)
{
    const vec4 clipSpacePos = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * vec4(fragPos3D, 1.0);
    return clipSpacePos.z / clipSpacePos.w;
}

float ComputeLinearDepth(vec3 fragPos3D) {
    const vec4 clipSpacePos = u_GlobalCameraData.Projection * u_GlobalCameraData.InverseView * vec4(fragPos3D.xyz, 1.0);
    const float clipSpaceDepth = (clipSpacePos.z / clipSpacePos.w) * 2.0 - 1.0; // put back between -1 and 1
    const float linearDepth = (2.0 * inNearPoint.z * inFarPoint.z) / (inFarPoint.z + inNearPoint.z - clipSpaceDepth * (inFarPoint.z - inNearPoint.z)); // get linear value between 0.01 and 100
    return linearDepth / inFarPoint.z; // normalize
}

void main() {
    const float t = -inNearPoint.y/(inFarPoint.y-inNearPoint.y);
    const vec3 fragPos3D = inNearPoint + t * (inFarPoint - inNearPoint);
    
    gl_FragDepth = ComputeDepth(fragPos3D);
    
    const float linearDepth = ComputeLinearDepth(fragPos3D);
    const float fading = max(0, (0.5 - linearDepth));

    outColor = (Gridify(fragPos3D, 10) + Gridify(fragPos3D, 1)) * float(t > 0);
    outColor.a *= fading;
}