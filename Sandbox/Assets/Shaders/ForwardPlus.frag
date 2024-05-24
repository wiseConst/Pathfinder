#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

#define PHONG 0
#define PBR 1

#if PHONG
#include "Include/PhongShading.glslh"
#endif

#if PBR
#include "Include/PBRShading.glslh"
#endif

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
    flat uint32_t MaterialBufferIndex;
    mat3 TBNtoWorld;
} i_VertexInput;

// https://forums.unrealengine.com/t/the-math-behind-combining-bc5-normals/365189
vec3 GetNormalFromNormalMap(const PBRData mat)
{
    vec2 NormalXY = texture(u_GlobalTextures[nonuniformEXT(mat.NormalTextureIndex)], i_VertexInput.UV).xy;
	
	NormalXY = NormalXY * vec2(2.0f,2.0f) - vec2(1.0f,1.0f);
	float NormalZ = sqrt( saturate( 1.0f - dot( NormalXY, NormalXY ) ) );
	return vec3( NormalXY.xy, NormalZ);
}

vec2 GetMetallicRoughnessMap(const PBRData mat)
{
    return texture(u_GlobalTextures[nonuniformEXT(mat.MetallicRoughnessTextureIndex)], i_VertexInput.UV).xy;
}

void main()
{
    const PBRData mat = s_GlobalMaterialBuffers[nonuniformEXT(i_VertexInput.MaterialBufferIndex)].mat;

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
    #if PBR
    const vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    const vec3 ambient = albedo.rgb * ao * .04f;
    irradiance += (LightData(u_PC.LightDataBuffer).DirectionalLightCount + LightData(u_PC.LightDataBuffer).PointLightCount + LightData(u_PC.LightDataBuffer).SpotLightCount) > 0 ? ambient : vec3(0);
    #endif

    for(uint i = 0; i < LightData(u_PC.LightDataBuffer).DirectionalLightCount; ++i)
    {
        DirectionalLight dl = LightData(u_PC.LightDataBuffer).DirectionalLights[i];

        const float kShadow = 1.0f;
        #if PHONG
            irradiance += DirectionalLightContribution(kShadow, V, N, dl, albedo.rgb, ao);
        #elif PBR
            irradiance += DirectionalLightContribution(kShadow, F0, V, N, dl, albedo.rgb, roughness, metallic);
        #endif
    }

    const uint linearTileIndex = GetLinearGridIndex(gl_FragCoord.xy, CameraData(u_PC.CameraDataBuffer).FullResolution.x);
    // Point lights
    {
        const uint offset = linearTileIndex * MAX_POINT_LIGHTS;
        for(uint i = 0; i < LightData(u_PC.LightDataBuffer).PointLightCount && VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i) 
        {
            const uint lightIndex = VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i];
            if (lightIndex >= LightData(u_PC.LightDataBuffer).PointLightCount) continue;

            PointLight pl = LightData(u_PC.LightDataBuffer).PointLights[lightIndex];
         const float kShadow = 1.0f;
           // if(pl.bCastShadows > 0) kShadow = 1.f - PointShadowCalculation(u_PointShadowmap[lightIndex], i_VertexInput.WorldPos, u_PC.CameraDataBuffer.Position, pl, u_PC.pad0.x /* far plane for point light shadow maps */);

            #if PHONG
                irradiance += PointLightContribution(kShadow, i_VertexInput.WorldPos, N, V, pl, albedo.rgb, ao);
            #elif PBR
                irradiance += PointLightContribution(kShadow, i_VertexInput.WorldPos,F0,  N, V, pl, albedo.rgb, roughness, metallic);
            #endif
        }
    }
    
    // Spot lights
    {
        const uint offset = linearTileIndex * MAX_SPOT_LIGHTS;
        for(uint i = 0; i < LightData(u_PC.LightDataBuffer).SpotLightCount && VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i)
        {
           const uint lightIndex = VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i];
           if (lightIndex >= LightData(u_PC.LightDataBuffer).SpotLightCount) continue;

           SpotLight spl = LightData(u_PC.LightDataBuffer).SpotLights[lightIndex];
           #if PHONG
               irradiance += SpotLightContribution(i_VertexInput.WorldPos, N, V, spl, albedo.rgb, ao);
           #elif PBR
               irradiance += SpotLightContribution(i_VertexInput.WorldPos, F0, N, V, spl, albedo.rgb, roughness, metallic);
           #endif
        }
    }

    outFragColor = vec4(irradiance, albedo.a);

    outBrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    if(dot(outFragColor.rgb, vec3(0.2126, 0.7152, 0.0722)) > 1.f)
        outBrightColor = vec4(outFragColor.rgb, 1.0);

    if(bRenderViewNormalMap)
    {
        outViewNormalMap = CameraData(u_PC.CameraDataBuffer).View * vec4(N, 1);
    }
}