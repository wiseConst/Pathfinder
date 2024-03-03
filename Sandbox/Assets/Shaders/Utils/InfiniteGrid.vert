#version 460

layout(location = 0) out vec3 outNearPoint;
layout(location = 1) out vec3 outFarPoint;

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

const vec3 gridPlanes[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, 1, 0), vec3(-1, -1, 0),
    vec3(-1, -1, 0), vec3(1, -1, 0), vec3(1, 1, 0)
);

vec3 UnprojectPoint(vec3 p, mat4 invView, mat4 projection)
{
    const mat4 srcView = inverse(invView);
    const mat4 invProj = inverse(projection);
    vec4 worldP = srcView * invProj * vec4(p, 1.0);  
    return worldP.xyz / worldP.w;
}

void main()
{
    const vec3 p = gridPlanes[gl_VertexIndex].xyz;
    outNearPoint = UnprojectPoint(vec3(p.x, p.y, 0.0), u_GlobalCameraData.View, u_GlobalCameraData.Projection); // unproject on the near plane
    outFarPoint = UnprojectPoint(vec3(p.x, p.y, 1.0), u_GlobalCameraData.View, u_GlobalCameraData.Projection); // unproject on the far plane
    gl_Position = vec4(p, 1.0); // using directly the clipped coordinates
}