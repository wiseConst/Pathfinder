#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const int blurRange = 2;
layout(set = LAST_BINDLESS_SET + 1, binding = 0) uniform sampler2D u_AO;

void main()
{
	const vec2 texelSize = 1.f / textureSize(u_AO, 0).xy;
	float accumulatedAO = 0.f;
	uint accumulatedCount = 0;
	for(int x = -blurRange; x < blurRange; ++x)
	{
		for(int y = -blurRange; y < blurRange; ++y)
		{
			const vec2 offsetUV = vec2(float(x), float(y)) * texelSize;
			accumulatedAO += texture(u_AO, inUV + offsetUV).r;
			++accumulatedCount;
		}
	}
	outFragColor = accumulatedAO / accumulatedCount;
}