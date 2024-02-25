#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(set = LAST_BINDLESS_SET + 1, binding = 0) uniform sampler2D u_NoiseTex;
layout(set = LAST_BINDLESS_SET + 1, binding = 1) uniform sampler2D u_DepthTex;

float ComputeAO()
{
	return 0.f;
}

void main()
{
    // Rotation vector for kernel samples current fragment will be used to create TBN -> View Space
    const uvec2 noiseScale = uvec2(textureSize(u_DepthTex, 0)) / uvec2(textureSize(u_NoiseTex, 0));
    const vec3 randomVec = normalize(texture(u_NoiseTex, inUV * noiseScale).xyz);
    
	float result = 0.f;
	

	outFragColor = 1.f - result;
}