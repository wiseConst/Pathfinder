#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const int blurRange = 2;
layout(constant_id = 1) const float blurPower = 0.85f;

void main()
{
	const vec2 texelSize = 1.f / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0).xy;
	float accumulatedAO = 0.f;
	uint accumulatedCount = 0;
	for(int x = -blurRange; x < blurRange; ++x)
	{
		for(int y = -blurRange; y < blurRange; ++y)
		{
			const vec2 offsetUV = vec2(float(x), float(y)) * texelSize;
			accumulatedAO += blurPower * texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV + offsetUV).r;
			++accumulatedCount;
		}
	}
	outFragColor = accumulatedAO / accumulatedCount;
}