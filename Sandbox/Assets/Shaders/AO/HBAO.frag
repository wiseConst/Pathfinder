#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

/* NOTE: 
    From PushConstantBlock:
      uint32_t StorageImageIndex    - u_NoiseTex
      uint32_t AlbedoTextureIndex   - u_DepthTex
      uint32_t MeshIndexBufferIndex - u_ViewNormalMap
*/

layout(constant_id = 0) const float Radius = 0.5f;
layout(constant_id = 1) const float AngleBias = 30.0f;
layout(constant_id = 2) const float DegToRad = 0.01745328f;
layout(constant_id = 3) const uint SampleCount = 6;

vec3 ReconstructViewNormal(const vec3 viewPos)
{
    // The dFdy and dFdX are glsl functions used to calculate two vectors in view space 
    // that lie on the plane of the surface being drawn. We pass the view space position to these functions.
    // The cross product of these two vectors give us the normal in view space.
    vec3 viewNormal = cross(dFdy(viewPos), dFdx(viewPos));
       
    // The normal is initilly away from the screen based on the order in which we calculate the cross products. 
    // Here, we need to invert it to point towards the screen by multiplying by -1.
    viewNormal = normalize(viewNormal * -1.0);
    return viewNormal;
}

float saturate(float a)
{
	return clamp(a, 0.0, 1.0);
}

const float INFINITY = 1.f / 0.f;
float ComputeAO(const vec3 N, const vec2 direction, const vec2 screenSize, const vec3 viewPos, const vec2 invRes)
{
    // 1. Calculate tangent vector based on (earlier it was normal, but viewVector gives better results) and XY-plane: "left-direction" cuz direction we passed has no Z.
    // idk wtf is here, the way I understand should be:
    //const vec3 leftDirection = normalize(cross(N, vec3(direction, 0)));
    //const vec3 T = cross(leftDirection, N);
    const vec3 viewVector = normalize(viewPos);
	const vec3 leftDirection = normalize(cross(viewVector, vec3(direction, 0)));
	vec3 projectedNormal = N - dot(leftDirection, N) * leftDirection;
    float projectedLen = length(projectedNormal);
	projectedNormal /= projectedLen;
	const vec3 T = cross(projectedNormal, leftDirection);

    // 2. Calculate tangent angle, will be used in the end. Also apply some angle bias, to prevent "false occlusions".
    const float bias = AngleBias * DegToRad;
    const float Tangle = atan(T.z / length(T.xy)) + bias;

    // 3. Find max horizon vector.
    vec3 maxHorizonPos = vec3(0, 0, -INFINITY);
    const vec2 texelSize = 1.f / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0);
    vec3 lastDiff = vec3(0);
    for(uint i = 1; i < SampleCount + 1; ++i)
    {
        const vec2 offsetUV = inUV + i * direction * texelSize;

        const vec3 sampledViewPos = ScreenSpaceToView(vec4(offsetUV, texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], offsetUV).r, 1), invRes).xyz;

        const vec3 diff = sampledViewPos - viewPos;
        if(sampledViewPos.z > maxHorizonPos.z && length(diff) < Radius)
        {
           maxHorizonPos = sampledViewPos;
           lastDiff = diff;
        }
    }

    // 4. Calculate horizon angle.
    const vec3 horizonVec = maxHorizonPos - viewPos;
    const float Hangle = atan(horizonVec.z / length(horizonVec.xy));
            
    const float norm = length(lastDiff) / Radius;
    const float attenuation = 1 - norm * norm;

    return attenuation *  saturate(sin(Hangle) - sin(Tangle)) / projectedLen;
}

void main()
{
    const uvec2 noiseScale = uvec2(u_GlobalCameraData.FullResolution / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], 0));
    const vec3 randomVec = normalize(texture(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], inUV * noiseScale).xyz);
    const vec2 invRes = 1.0f / u_GlobalCameraData.FullResolution;

    const vec3 viewPos = ScreenSpaceToView(vec4(inUV, texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], inUV).r, 1), invRes).xyz;
    const vec3 N = ReconstructViewNormal(viewPos);

	float result = 0.f;
	result += ComputeAO(N, vec2(randomVec), u_GlobalCameraData.FullResolution, viewPos, invRes);
	result += ComputeAO(N, -vec2(randomVec), u_GlobalCameraData.FullResolution, viewPos, invRes);
	result += ComputeAO(N, vec2(-randomVec.y, randomVec.x), u_GlobalCameraData.FullResolution, viewPos, invRes);
	result += ComputeAO(N, vec2(randomVec.y, -randomVec.x), u_GlobalCameraData.FullResolution, viewPos, invRes);

    const float ao = 1.f - result / 4;
	outFragColor = pow(ao, 4);
}