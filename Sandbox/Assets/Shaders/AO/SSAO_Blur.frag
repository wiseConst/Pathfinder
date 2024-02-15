#version 460

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const int blurRange = 2;
layout(set = 0, binding = 0) uniform sampler2D u_SSAO;

void main()
{
	const vec2 texelSize = 1.f / textureSize(u_SSAO, 0).xy;
	float accumulatedAO=0.F;
	uint accumulatedCount=0;
	for(int x = -blurRange; x < blurRange; ++x)
	{
		for(int y = -blurRange; y < blurRange; ++y)
		{
			const vec2 offsetUV = vec2(float(x), float(y)) * texelSize;
			accumulatedAO += texture(u_SSAO, inUV + offsetUV).r;
			++accumulatedCount;
		}
	}
	outFragColor = accumulatedAO / accumulatedCount;
}