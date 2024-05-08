#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out vec4 outFragColor;
layout(location = 0) in vec2 inUV;

#define REINHARD_TONEMAPPING 1
#define EXPOSURE 1.0f
#define GAMMA 2.2f

void main()
{
    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], inUV);
    const vec3 bloomBlur = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV).rgb;
    const vec3 hdrColor = albedo.rgb + bloomBlur;

    // TODO: Replace with exposure
    // reinhard tone-mapping
    #if REINHARD_TONEMAPPING
    vec3 finalColor = hdrColor / (hdrColor + vec3(1.0));
    #else
    vec3 finalColor = vec3(1.f) - exp(-hdrColor * EXPOSURE);
    #endif

    // gamma correction 
    finalColor = pow(finalColor, vec3(1.0 / GAMMA));

    outFragColor = vec4(finalColor, albedo.a);
}