#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out vec4 outFragColor;
  
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const uint32_t BlurDirection = 0;
  
const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
const float blurScale = 1.05f;
const float blurStrength = 1.05f;

void main()
{
    const vec2 texelSize = 1.0 / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0) * blurScale; // gets size of single texel
    vec3 result = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV).rgb * weight[0]; // current fragment's contribution
    for(int i = 1; i < 5; ++i)
    {
        if(BlurDirection == 0)
        {
            result += texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV + vec2(texelSize.x * i, 0.0)).rgb * weight[i] * blurStrength;
            result += texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV - vec2(texelSize.x * i, 0.0)).rgb * weight[i] * blurStrength;
        }
        else
        {
              result += texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV + vec2(0.0, texelSize.y * i)).rgb * weight[i] * blurStrength;
              result += texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV - vec2(0.0, texelSize.y * i)).rgb * weight[i] * blurStrength;
        }
    }

    outFragColor = vec4(result, 1.0);
}