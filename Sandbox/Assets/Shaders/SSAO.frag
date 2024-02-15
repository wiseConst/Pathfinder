#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(set = LAST_BINDLESS_SET + 1, binding = 0) uniform sampler2D u_NoiseTex;
layout(set = LAST_BINDLESS_SET + 1, binding = 1) uniform sampler2D u_DepthTex;

// Pregenerated by me samples that are in TBN space
const uint MAX_KERNEL_SIZE = 16;
const vec3 kernel[MAX_KERNEL_SIZE] = {
        vec3(0.0503445, 0.0217214, 0.0641986),   vec3(0.00837404, 0.0667834, 0.05623),
        vec3(0.0224165, 0.0189854, 0.0325471),   vec3(0.00594137, -0.00307045, 0.00331808),
        vec3(-0.0159512, -0.0311123, 0.0659393), vec3(0.00516041, 0.055751, 0.070698),
        vec3(0.154211, -0.0388485, 0.102913),    vec3(-0.163676, -0.0644617, 0.0140806),
        vec3(0.289096, -0.114518, 0.00878639),   vec3(0.191679, -0.186311, 0.201556),
        vec3(0.188451, -0.0166253, 0.308235),    vec3(0.27503, 0.0200761, 0.226216),
        vec3(0.18431, 0.290627, 0.0778982),      vec3(0.518095, 0.0435445, 0.0835122),
        vec3(-0.0664, -0.306636, 0.20302),       vec3(0.411915, -0.471147, 0.198689)
};

const float INV_MAX_KERNEL_SIZE_F = 1./float(MAX_KERNEL_SIZE);
const float sampleRadius = .873f;
const float sampleBias = .025f;

vec3 GetViewPositionFromDepth(vec2 coords) {
        const float fragmentDepth = texture(u_DepthTex, coords).r;
      
        // screen space -> ndc
        const vec4 ndc = vec4(coords.x * 2.0 - 1.0, coords.y * 2.0 - 1.0, fragmentDepth * 2.0 - 1.0, 1.0);
        
        // ndc -> view space
        vec4 posVS = u_GlobalCameraData.InverseProjection * ndc;
      
        // Since we used a projection transformation (even if it was in inverse)
        // we need to convert our homogeneous coordinates using the perspective divide.
        return posVS.xyz / posVS.w;
}

// NOTE: John Chapman, learnopengl, https://betterprogramming.pub/depth-only-ssao-for-forward-renderers-1a3dcfa1873a

void main()
{
    // Rotation vector for kernel samples current fragment will be used to create TBN -> View Space
    const uvec2 noiseScale = uvec2(textureSize(u_DepthTex, 0)) / uvec2(textureSize(u_NoiseTex, 0));
    const vec3 randomVec = normalize(texture(u_NoiseTex, inUV * noiseScale).xyz);

    const vec3 viewPos = GetViewPositionFromDepth(inUV);
    // The dFdy and dFdX are glsl functions used to calculate two vectors in view space 
    // that lie on the plane of the surface being drawn. We pass the view space position to these functions.
    // The cross product of these two vectors give us the normal in view space.
    vec3 viewNormal = cross(dFdy(viewPos.xyz), dFdx(viewPos.xyz));

    // The normal is initilly away from the screen based on the order in which we calculate the cross products. 
    // Here, we need to invert it to point towards the screen by multiplying by -1.
    viewNormal = normalize(viewNormal * -1.F);

    // NOTE: Construct tangent based on randomVec, Gramm-Schmidt reorthogonalization along viewNormal. It's gonna offset randomVec so it'll be orthogonal to viewNormal.
    const vec3 T = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    const vec3 B = cross(viewNormal, T);
    const mat3 TBN = mat3(T, B, viewNormal); 
    float occlusion = .0F;
    for (int i = 0 ; i < MAX_KERNEL_SIZE ; i++) {
        vec3 samplePos = TBN * kernel[i];
        samplePos = viewPos + samplePos * sampleRadius;

        const vec3 sampleDir = normalize(samplePos - viewPos); // view space sample direction
        const float NdotS = max(dot(viewNormal, sampleDir), 0); // to make sure that the angle between normal and sample direction is not obtuse(>90 degrees)
		if(NdotS < 0.15) continue; // also if sampleDir is almost orthogonal it sucks

        vec4 offsetUV = vec4(samplePos, 1.0f);
        offsetUV = u_GlobalCameraData.Projection * offsetUV; // view -> clip ([-w, w])
        offsetUV.xy = (offsetUV.xy / offsetUV.w) * .5f + .5f; // clip -> ndc [-1 ,1] -> UV space x, y:[0, 1]
        
        const float sampledGeometryDepth = GetViewPositionFromDepth(offsetUV.xy).z;
        const float rangeCheck = smoothstep(0.0F, 1.F, sampleRadius / abs(viewPos.z - sampledGeometryDepth)); // in case rendered fragment is further than generated sample, then sample is not occluded
        
        // NOTE: Working in view space(looking towards negative Z axis, so greater value means closer to the cam).
        occlusion += float(sampledGeometryDepth >= samplePos.z + sampleBias) * rangeCheck * NdotS;
    }

    const float visibility = 1.0 - occlusion * INV_MAX_KERNEL_SIZE_F; // From [0, MAX_KERNEL_SIZE] -> [0, 1]
    outFragColor =  pow(visibility, 7);
}