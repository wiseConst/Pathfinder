#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const bool bUseViewNormalMap = false;
layout(constant_id = 1) const uint32_t occlusionPower = 1;
layout(constant_id = 2) const float sampleRadius = .285f;
layout(constant_id = 3) const float sampleBias = .025f;
layout(constant_id = 4) const uint32_t SAMPLE_COUNT = 16;


/* NOTE: 
    From PushConstantBlock:
      uint32_t StorageImageIndex    - u_ViewNormalMap
      uint32_t AlbedoTextureIndex   - u_DepthTex
*/

// Precalculated hemisphere kernel(TBN space)
const uint KERNEL_SIZE = 32;
const vec3 g_Kernel[KERNEL_SIZE] = {
		vec3(-0.668154f, -0.084296f, 0.219458f),
		vec3(-0.092521f,  0.141327f, 0.505343f),
		vec3(-0.041960f,  0.700333f, 0.365754f),
		vec3( 0.722389f, -0.015338f, 0.084357f),
		vec3(-0.815016f,  0.253065f, 0.465702f),
		vec3( 0.018993f, -0.397084f, 0.136878f),
		vec3( 0.617953f, -0.234334f, 0.513754f),
		vec3(-0.281008f, -0.697906f, 0.240010f),
		vec3( 0.303332f, -0.443484f, 0.588136f),
		vec3(-0.477513f,  0.559972f, 0.310942f),
		vec3( 0.307240f,  0.076276f, 0.324207f),
		vec3(-0.404343f, -0.615461f, 0.098425f),
		vec3( 0.152483f, -0.326314f, 0.399277f),
		vec3( 0.435708f,  0.630501f, 0.169620f),
		vec3( 0.878907f,  0.179609f, 0.266964f),
		vec3(-0.049752f, -0.232228f, 0.264012f),
		vec3( 0.537254f, -0.047783f, 0.693834f),
		vec3( 0.001000f,  0.177300f, 0.096643f),
		vec3( 0.626400f,  0.524401f, 0.492467f),
		vec3(-0.708714f, -0.223893f, 0.182458f),
		vec3(-0.106760f,  0.020965f, 0.451976f),
		vec3(-0.285181f, -0.388014f, 0.241756f),
		vec3( 0.241154f, -0.174978f, 0.574671f),
		vec3(-0.405747f,  0.080275f, 0.055816f),
		vec3( 0.079375f,  0.289697f, 0.348373f),
		vec3( 0.298047f, -0.309351f, 0.114787f),
		vec3(-0.616434f, -0.117369f, 0.475924f),
		vec3(-0.035249f,  0.134591f, 0.840251f),
		vec3( 0.175849f,  0.971033f, 0.211778f),
		vec3( 0.024805f,  0.348056f, 0.240006f),
		vec3(-0.267123f,  0.204885f, 0.688595f),
		vec3(-0.077639f, -0.753205f, 0.070938f)
};

// Samples from crysis's rotation texture
const uint ROT_SIZE = 16;
const vec3 g_RotationVectors[ROT_SIZE] = {
    vec3(0.15332, 0.10449, 0.9375),
    vec3(0.05762, 0.06494, 0.41797),
    vec3(0.05615, 0.12207, 0.10205),
    vec3(0.10205, 0.31836, 0.02625),
    vec3(0.01099, 0.35156, 0.36719),
    vec3(0.0481, 0.50781, 0.4082),
    vec3(0.03076, 0.3418, 0.39063),
    vec3(0.09766, 0.26172, 0.13281),
    vec3(0.20508, 0.32227, 0.07227),
    vec3(0.51953, 0.33203, 0.2832),
    vec3(0.35156, 0.25391, 0.77734),
    vec3(0.27539, 0.18164, 0.22656),
    vec3(0.92188, 0.00519, 0.05957),
    vec3(0.90625, 0.03564, 0.23438),
    vec3(0.53516, 0.01099, 0.63672),
    vec3(0.16797, 0.008, 0.10938)
};

vec3 GetViewPositionFromDepth(const vec2 coords) {
    // UV space -> ndc
    // NOTE: For OpenGL should be (fragmentDepth * 2.f - .1f)
    const vec4 ndc = vec4(coords * 2.f - 1.f, texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], coords).r, 1.f);
    
    // ndc -> view space
    vec4 posVS = CameraData(u_PC.CameraDataBuffer).InverseProjection * ndc;
    
    // Since we used a projection transformation (even if it was in inverse)
    // we need to convert our homogeneous coordinates using the perspective divide.
    return posVS.xyz / posVS.w;
}

void main()
{
    // Rotation vector for kernel samples current fragment will be used to create TBN -> View Space
    const uvec2 noiseScale = uvec2(textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0)) / 4;

    const ivec2 rotationUV = ivec2(inUV * noiseScale) % 4;
    const vec3 randomVec = normalize(g_RotationVectors[rotationUV.y * 4 + rotationUV.x]);

    const vec3 viewPos = GetViewPositionFromDepth(inUV);

    vec3 viewNormal = vec3(.0f);
    if (bUseViewNormalMap)
    {
        viewNormal = texture(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], inUV).xyz;
    } else {
        viewNormal = normalize(cross(dFdx(viewPos), dFdy(viewPos)));
    }

    // NOTE: Construct tangent based on randomVec, Gramm-Schmidt reorthogonalization along viewNormal. It's gonna offset randomVec so it'll be orthogonal to viewNormal.
    const vec3 T = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    const vec3 B = cross(viewNormal, T);
    const mat3 TBN = mat3(T, B, viewNormal); 
    float occlusion = 0.0f;
    for (uint i = 0; i < SAMPLE_COUNT ; i++) {
        vec3 samplePos = TBN * g_Kernel[i];
        samplePos = viewPos + samplePos * sampleRadius;

        vec4 offsetUV = vec4(samplePos, 1.0f);
        offsetUV = CameraData(u_PC.CameraDataBuffer).Projection * offsetUV; // view -> clip ([-w, w])
        offsetUV.xy = (offsetUV.xy / offsetUV.w) * .5f + .5f; // clip -> ndc [-1 ,1] -> UV space x, y:[0, 1]
        
        const float sampledGeometryDepth = ScreenSpaceDepthToView(texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], offsetUV.xy).r);
        const float rangeCheck = smoothstep(0.0f, 1.0f, sampleRadius / abs(viewPos.z - sampledGeometryDepth)); // in case rendered fragment is further than generated sample, then sample is not occluded
        
        // NOTE: Working in view space(looking towards negative Z axis, so greater value means closer to the cam).
        occlusion += step(samplePos.z + sampleBias, sampledGeometryDepth) * rangeCheck;
    }
    
    occlusion = 1.0 - occlusion / float(SAMPLE_COUNT); // From [0, MAX_KERNEL_SIZE] -> [0, 1]
    outFragColor = pow(abs(occlusion), occlusionPower);
}