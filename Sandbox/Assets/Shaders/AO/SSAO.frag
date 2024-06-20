#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out float outFragColor;
layout(location = 0) in vec2 inUV;

layout(constant_id = 0) const bool bUseViewNormalMap = false;
layout(constant_id = 1) const uint32_t occlusionPower = 3;
layout(constant_id = 2) const float sampleRadius = .555f;
layout(constant_id = 3) const float sampleBias = .025f;


/* NOTE: 
    From PushConstantBlock:
      uint32_t StorageImageIndex    - u_NoiseTex
      uint32_t AlbedoTextureIndex   - u_DepthTex
      uint32_t data0.x - u_ViewNormalMap
*/

// Pregenerated by me samples that are in TBN space
const uint KERNEL_SIZE = 16;
const vec3 g_Kernel[KERNEL_SIZE] = {
        vec3(0.0503445, 0.0217214, 0.0641986),   vec3(0.00837404, 0.0667834, 0.05623),
        vec3(0.0224165, 0.0189854, 0.0325471),   vec3(0.00594137, -0.00307045, 0.00331808),
        vec3(-0.0159512, -0.0311123, 0.0659393), vec3(0.00516041, 0.055751, 0.070698),
        vec3(0.154211, -0.0388485, 0.102913),    vec3(-0.163676, -0.0644617, 0.0140806),
        vec3(0.289096, -0.114518, 0.00878639),   vec3(-0.191679, 0.186311, 0.201556),
        vec3(-0.188451, 0.0166253, 0.308235),    vec3(0.27503, 0.0200761, 0.226216),
        vec3(0.18431, 0.290627, 0.0778982),      vec3(-0.518095, 0.0435445, 0.0835122),
        vec3(-0.0664, -0.306636, 0.20302),       vec3(-0.411915, 0.471147, 0.198689)
};

const float INV_KERNEL_SIZE_F = 1./float(KERNEL_SIZE);

vec3 GetViewPositionFromDepth(vec2 coords) {
    // UV space -> ndc
    // NOTE: For OpenGL should be (fragmentDepth * 2.f - .1f)
    const vec4 ndc = vec4(coords * 2.f - 1.f, texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], coords).r, 1.f);
    
    // ndc -> view space
    vec4 posVS = CameraData(u_PC.CameraDataBuffer).InverseProjection * ndc;
    
    // Since we used a projection transformation (even if it was in inverse)
    // we need to convert our homogeneous coordinates using the perspective divide.
    return posVS.xyz / posVS.w;
}

// NOTE: John Chapman, learnopengl, https://betterprogramming.pub/depth-only-ssao-for-forward-renderers-1a3dcfa1873a
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

void main()
{
    // Rotation vector for kernel samples current fragment will be used to create TBN -> View Space
    const uvec2 noiseScale = uvec2(textureSize(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], 0)) / uvec2(textureSize(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], 0));
    const vec3 randomVec = normalize(texture(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], inUV * noiseScale).xyz);

    const vec3 viewPos = GetViewPositionFromDepth(inUV);

    vec3 viewNormal = vec3(.0f);
    if (bUseViewNormalMap)
    {
        viewNormal = texture(u_GlobalTextures[nonuniformEXT(uint32_t(u_PC.data0.x))], inUV).xyz;
    } else {
        viewNormal = ReconstructViewNormal(viewPos);
    }

    // NOTE: Construct tangent based on randomVec, Gramm-Schmidt reorthogonalization along viewNormal. It's gonna offset randomVec so it'll be orthogonal to viewNormal.
    const vec3 T = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    const vec3 B = cross(viewNormal, T);
    const mat3 TBN = mat3(T, B, viewNormal); 
    float occlusion = .0F;
    for (int i = 0 ; i < KERNEL_SIZE ; i++) {
        vec3 samplePos = TBN * g_Kernel[i];
        samplePos = viewPos + samplePos * sampleRadius;

        const vec3 sampleDir = normalize(samplePos - viewPos); // view space sample direction
        vec4 offsetUV = vec4(samplePos, 1.0f);
        offsetUV = CameraData(u_PC.CameraDataBuffer).Projection * offsetUV; // view -> clip ([-w, w])
        offsetUV.xy = (offsetUV.xy / offsetUV.w) * .5f + .5f; // clip -> ndc [-1 ,1] -> UV space x, y:[0, 1]
        
        const float sampledGeometryDepth = GetViewPositionFromDepth(offsetUV.xy).z;
        const float rangeCheck = smoothstep(0.0F, 1.F, sampleRadius / abs(viewPos.z - sampledGeometryDepth)); // in case rendered fragment is further than generated sample, then sample is not occluded
        
        // NOTE: Working in view space(looking towards negative Z axis, so greater value means closer to the cam).
        occlusion += step(samplePos.z + sampleBias, sampledGeometryDepth) * rangeCheck;
    }

    occlusion = 1.0 - occlusion * INV_KERNEL_SIZE_F; // From [0, MAX_KERNEL_SIZE] -> [0, 1]
    outFragColor = pow(abs(occlusion), occlusionPower);
}