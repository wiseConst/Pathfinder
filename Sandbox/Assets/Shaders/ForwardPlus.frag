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

#ifdef PFR_RENDER_OPAQUE_EARLY_FRAGMENT_TESTS
layout(early_fragment_tests) in;
#endif

layout(constant_id = 0) const bool bRenderViewNormalMap = false;
/* NOTE: 
    From PushConstantBlock:
    uint32_t StorageImageIndex; - u_AO
    uint32_t AlbedoTextureIndex; - storing cascade debug image

    vec2 pad0; x(far plane for point light shadow maps)
*/

layout(set = LAST_BINDLESS_SET + 3, binding = 0) uniform sampler2DArray u_DirShadowmap[MAX_DIR_LIGHTS];

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
    const vec3 V = normalize(u_PC.CameraDataBuffer.Position - i_VertexInput.WorldPos);

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
    irradiance += (u_PC.LightDataBuffer.DirectionalLightCount + u_PC.LightDataBuffer.PointLightCount + u_PC.LightDataBuffer.SpotLightCount) > 0 ? ambient : vec3(0);
    #endif
    
    for(uint i = 0; i < u_PC.LightDataBuffer.DirectionalLightCount; ++i)
    {
        DirectionalLight dl = u_PC.LightDataBuffer.DirectionalLights[i];

        float kShadow = 1.0f;
        if (dl.bCastShadows > 0) {
            const vec4 fragPosVS = u_PC.CameraDataBuffer.View * vec4(i_VertexInput.WorldPos, 1);
            const float depthVS = abs(fragPosVS.z);

            int layer = -1;
            for(int cascadeIndex = 0; cascadeIndex < MAX_SHADOW_CASCADES - 1; ++cascadeIndex)
            {
                if(depthVS < u_PC.LightDataBuffer.CascadePlaneDistances[cascadeIndex])
                {
                    layer = cascadeIndex;
                    break;
                }
            }

            bool bLayerNeedsFix = layer == -1;
            if(layer == -1) layer = int(MAX_SHADOW_CASCADES - 1);

         #if 0
            vec3 cascadeColor=vec3(0);
            if(layer == 0) cascadeColor.r = 1;
            else if(layer==1) cascadeColor.g = 1;
            else if(layer==2) cascadeColor.b = 1;
            else if(layer==3) cascadeColor=vec3(1,1,0);
            
            imageStore(u_GlobalImages_RGBA8[u_PC.AlbedoTextureIndex], ivec2(gl_FragCoord.xy), vec4(cascadeColor,1));
         #endif

            float biasMultiplier = 1.f;
            if (layer == MAX_SHADOW_CASCADES - 1)
            {
                biasMultiplier = 1.f / (u_PC.CameraDataBuffer.zFar * .5f);
            }
            else
            {
                biasMultiplier = 1.f / (u_PC.LightDataBuffer.CascadePlaneDistances[layer] * .5f);
            }
            kShadow = 1.f - DirShadowCalculation(u_DirShadowmap[i], biasMultiplier, layer, u_PC.LightDataBuffer.DirLightViewProjMatrices[i * MAX_DIR_LIGHTS + layer + int(bLayerNeedsFix)] * vec4(i_VertexInput.WorldPos, 1), N, normalize(dl.Direction));
        }

        #if PHONG
            irradiance += DirectionalLightContribution(kShadow, V, N, dl, albedo.rgb, ao);
        #elif PBR
            irradiance += DirectionalLightContribution(kShadow, F0, V, N, dl, albedo.rgb, roughness, metallic);
        #endif
    }
    
    const uint linearTileIndex = GetLinearGridIndex(gl_FragCoord.xy, u_PC.CameraDataBuffer.FullResolution.x);
    // Point lights
    {
        const uint offset = linearTileIndex * MAX_POINT_LIGHTS;
        for(uint i = 0; i < u_PC.LightDataBuffer.PointLightCount && VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i) 
        {
            const uint lightIndex = VisiblePointLightIndicesBuffer(u_PC.VisiblePointLightIndicesDataBuffer).indices[offset + i];
            if (lightIndex >= u_PC.LightDataBuffer.PointLightCount) continue;

            PointLight pl = u_PC.LightDataBuffer.PointLights[lightIndex];
            float kShadow = 1.0f;
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
        for(uint i = 0; i < u_PC.LightDataBuffer.SpotLightCount && VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i] != INVALID_LIGHT_INDEX; ++i)
        {
           const uint lightIndex = VisibleSpotLightIndicesBuffer(u_PC.VisibleSpotLightIndicesDataBuffer).indices[offset + i];
           if (lightIndex >= u_PC.LightDataBuffer.SpotLightCount) continue;

           SpotLight spl = u_PC.LightDataBuffer.SpotLights[lightIndex];
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
        outViewNormalMap = u_PC.CameraDataBuffer.View * vec4(N, 1);
    }
}