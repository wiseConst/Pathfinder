#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

#include "Include/PBRShading.glsl"

layout(constant_id = 0) const bool bRenderViewNormalMap = false;

/* NOTE: 
    From PushConstantBlock:
    uint32_t StorageImageIndex; - u_AO
*/

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec4 outBrightColor;
layout(location = 2) out vec4 outViewNormalMap;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    flat uint64_t MaterialBufferBDA;
    mat3 TBNtoWorld;
} i_VertexInput;

// https://forums.unrealengine.com/t/the-math-behind-combining-bc5-normals/365189
vec3 GetNormalFromNormalMap(const PBRData mat)
{
    vec2 NormalXY = texture(u_GlobalTextures[nonuniformEXT(mat.NormalTextureIndex)], i_VertexInput.UV).xy;
	
	NormalXY = NormalXY * vec2(2.f) - vec2(1.f);
	const float NormalZ = sqrt( saturate( 1.f - dot( NormalXY, NormalXY ) ) );
	return vec3( NormalXY.xy, NormalZ);
}

vec2 GetMetallicRoughnessMap(const PBRData mat)
{
    return texture(u_GlobalTextures[nonuniformEXT(mat.MetallicRoughnessTextureIndex)], i_VertexInput.UV).xy;
}

#define POISSONED_PCF 1
#define POISSON_PCF_SIZE 8

void main()
{
    uint i;
    const PBRData mat = MaterialBuffer(i_VertexInput.MaterialBufferBDA).mat;

    const vec3 N = normalize(i_VertexInput.TBNtoWorld * GetNormalFromNormalMap(mat));
    const vec3 V = normalize(CameraData(u_PC.CameraDataBuffer).Position - i_VertexInput.WorldPos);

    // <0> is reserved for white texture
    const vec3 emissive = mat.EmissiveTextureIndex != 0 ? texture(u_GlobalTextures[nonuniformEXT(mat.EmissiveTextureIndex)], i_VertexInput.UV).rgb : vec3(0.f);
    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(mat.AlbedoTextureIndex)], i_VertexInput.UV) * mat.BaseColor * i_VertexInput.Color;

    const vec2 screenSpaceUV = gl_FragCoord.xy / textureSize(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], 0).xy;
    const float ao = texture(u_GlobalTextures[nonuniformEXT(u_PC.StorageImageIndex)], screenSpaceUV).r * texture(u_GlobalTextures[nonuniformEXT(mat.OcclusionTextureIndex)], i_VertexInput.UV).r;

    const vec2 metallicRoughness = GetMetallicRoughnessMap(mat);
    const float metallic = metallicRoughness.g * mat.Metallic;
    const float roughness = metallicRoughness.r * mat.Roughness;

    vec3 irradiance = emissive;
    const vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    const vec3 ambient = albedo.rgb * ao * .04f;
    irradiance += (LightData(u_PC.LightDataBuffer).DirectionalLightCount + LightData(u_PC.LightDataBuffer).PointLightCount + LightData(u_PC.LightDataBuffer).SpotLightCount) > 0 ? ambient : vec3(0);

    // TODO: Clean up this mess into structures
    int cascade=0;
    for(i=0; i < LightData(u_PC.LightDataBuffer).DirectionalLightCount; ++i)
    {
        DirectionalLight dl = LightData(u_PC.LightDataBuffer).DirectionalLights[i];

        float kShadow = 0.0f; 
        if(dl.bCastShadows > 0)
        {
            const vec4 posVS = CameraData(u_PC.CameraDataBuffer).View * vec4(i_VertexInput.WorldPos, 1.0);
            const float depthVS = posVS.z; // negative since RH coord system
            CSMData csmData = CSMDataBuffer(u_PC.addr2).ShadowMapData;
            
            // Small optimization for cascade choosing
            const vec3 res = step(abs(vec3(csmData.CascadePlacementZ[0], csmData.CascadePlacementZ[1], csmData.CascadePlacementZ[2])), vec3(abs(depthVS)));
            const int layer = int(res.x + res.y + res.z);

            vec4 posLS = csmData.CascadeData[i].ViewProj[layer] * vec4(i_VertexInput.WorldPos, 1.0);
            posLS = (posLS / posLS.w);
            posLS.xy = vec2(posLS.x, -posLS.y) * 0.5f + 0.5f; // vk flipped Y viewport

            if(posLS.z > -1.0 && posLS.z < 1.0)
            {
                // calculate bias (based on depth map resolution and slope)
                const float bias = max(0.05 * (1.0 - dot(N, normalize(dl.Direction))), 0.005)
                 * when_eq(layer, SHADOW_CASCADE_COUNT - 1) * ( 1 / (CameraData(u_PC.CameraDataBuffer).zFar * 0.0625f) )
                 * when_neq(layer, SHADOW_CASCADE_COUNT - 1) * ( 1 / (csmData.CascadePlacementZ[layer] * 0.5f) );
     
                 // https://www.reddit.com/r/GraphicsProgramming/comments/r4xowh/cascaded_shadow_mapping_pcss_help
                 const float shadowDarkness = 0.8f;
                 const float transitionDistance = 25.0f;
                 const float shadowDistance = CameraData(u_PC.CameraDataBuffer).zFar;
                 const float shadowFade = saturate(1.0f - ((length(posVS) - (shadowDistance - transitionDistance)) / transitionDistance));

     #if POISSONED_PCF
               const vec2 texelSize = vec2(1.f) / textureSize(u_GlobalTextures[nonuniformEXT(csmData.CascadeData[i].CascadeTextureIndices[layer])], 0);
               for(uint k = 0; k < POISSON_PCF_SIZE; ++k)
               {
                       const uint index = POISSON_PCF_SIZE * GetUint32PRNG( i_VertexInput.WorldPos, k ) % POISSON_PCF_SIZE;
                       const float sampledDepth = texture(u_GlobalTextures[nonuniformEXT(csmData.CascadeData[i].CascadeTextureIndices[layer])], posLS.xy + SamplePoissonDisk(index) * texelSize).r;
                       kShadow += shadowDarkness * shadowFade * when_gt(step(sampledDepth + bias, posLS.z), 0.5f);
               }
               kShadow /= POISSON_PCF_SIZE;
     #else
               const float sampledDepth = texture(u_GlobalTextures[nonuniformEXT(csmData.CascadeData[i].CascadeTextureIndices[layer])], posLS.xy).r;
               kShadow += shadowDarkness * shadowFade * when_gt(step(sampledDepth + bias, posLS.z), 0.5f);
     #endif

            }
            cascade=layer;
        }

        irradiance += DirectionalLightContribution(1.f - kShadow, F0, V, N, dl, albedo.rgb, roughness, metallic);
    }

    const uint linearTileIndex = GetLinearGridIndex(gl_FragCoord.xy, CameraData(u_PC.CameraDataBuffer).FullResolution.x);
    // Point lights
    {
        const uint offset = linearTileIndex * MAX_POINT_LIGHTS;
        for(i = 0; i < LightData(u_PC.LightDataBuffer).PointLightCount && VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i) 
        {
            const uint lightIndex = VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i];
            if (lightIndex >= LightData(u_PC.LightDataBuffer).PointLightCount) continue;

            PointLight pl = LightData(u_PC.LightDataBuffer).PointLights[lightIndex];
            const float kShadow = 1.0f;
           // if(pl.bCastShadows > 0) kShadow = 1.f - PointShadowCalculation(u_PointShadowmap[lightIndex], i_VertexInput.WorldPos, u_PC.CameraDataBuffer.Position, pl, u_PC.pad0.x /* far plane for point light shadow maps */);

            irradiance += PointLightContribution(kShadow, i_VertexInput.WorldPos,F0,  N, V, pl, albedo.rgb, roughness, metallic);
        }
    }
    
    // Spot lights
    {
        const uint offset = linearTileIndex * MAX_SPOT_LIGHTS;
        for(i = 0; i < LightData(u_PC.LightDataBuffer).SpotLightCount && VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i)
        {
           const uint lightIndex = VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i];
           if (lightIndex >= LightData(u_PC.LightDataBuffer).SpotLightCount) continue;

           SpotLight spl = LightData(u_PC.LightDataBuffer).SpotLights[lightIndex];
           irradiance += SpotLightContribution(i_VertexInput.WorldPos, F0, N, V, spl, albedo.rgb, roughness, metallic);
        }
    }

    outFragColor = vec4(irradiance, albedo.a);
  
  #if 0
    if(cascade == 0) outFragColor.xyz *= vec3(1,0.1,0.1);
    else if(cascade == 1) outFragColor.xyz *= vec3(0.1,1,0.1);
    else if(cascade == 2) outFragColor.xyz *= vec3(0.1,0.1,1);
  #endif

    outBrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    if(dot(outFragColor.rgb, vec3(0.2126, 0.7152, 0.0722)) > 1.f)
        outBrightColor = vec4(outFragColor.rgb, 1.0);

    if(bRenderViewNormalMap)
    {
        outViewNormalMap = CameraData(u_PC.CameraDataBuffer).View * vec4(N, 1);
    }
}