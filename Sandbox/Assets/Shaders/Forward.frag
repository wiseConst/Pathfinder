#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"

#define PHONG 0
#define PBR 1

#if PHONG
#include "Assets/Shaders/Include/PhongShading.glslh"
#endif

#if PBR
#include "Assets/Shaders/Include/PBRShading.glslh"
#endif

layout(set = LAST_BINDLESS_SET + 1, binding = 0, scalar) buffer readonly VisiblePointLightIndicesBuffer
{
    uint32_t indices[];
} s_VisiblePointLightIndicesBuffer;
layout(set = LAST_BINDLESS_SET + 1, binding = 1, scalar) buffer readonly VisibleSpotLightIndicesBuffer
{
    uint32_t indices[];
} s_VisibleSpotLightIndicesBuffer;
layout(set = LAST_BINDLESS_SET + 1, binding = 2) uniform sampler2D u_AO;
layout(set = LAST_BINDLESS_SET + 1, binding = 3) uniform sampler2D u_DirShadowmap[MAX_DIR_LIGHTS];

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    mat3 TBNtoWorld;
} i_VertexInput;

vec3 GetNormalFromNormalMap(const PBRData mat)
{
    const vec3 normal = texture(u_GlobalTextures[nonuniformEXT(mat.NormalTextureIndex)], i_VertexInput.UV).xyz;
    return normal * 2.0 - 1.0;
}

void main()
{ 
    const PBRData mat = s_GlobalMaterialBuffers[u_PC.MaterialBufferIndex].mat;

    const vec3 N = normalize(i_VertexInput.TBNtoWorld * GetNormalFromNormalMap(mat));
    const vec3 V = normalize(u_GlobalCameraData.Position - i_VertexInput.WorldPos);

    // <0> is reserved for white texture
    const vec3 emissive = mat.EmissiveTextureIndex != 0 ? texture(u_GlobalTextures[nonuniformEXT(mat.EmissiveTextureIndex)], i_VertexInput.UV).rgb : vec3(0.f);
    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(mat.AlbedoTextureIndex)], i_VertexInput.UV) * mat.BaseColor;

    const vec2 screenSpaceUV = gl_FragCoord.xy / textureSize(u_AO, 0).xy;
    const float ao = texture(u_AO, screenSpaceUV).r * texture(u_GlobalTextures[nonuniformEXT(mat.OcclusionTextureIndex)], i_VertexInput.UV).r;

    const vec4 metallicRoughness = texture(u_GlobalTextures[nonuniformEXT(mat.MetallicRoughnessTextureIndex)], i_VertexInput.UV);
    const float metallic = metallicRoughness.b * mat.Metallic;
    const float roughness = metallicRoughness.g * mat.Roughness;

    vec3 irradiance = emissive;
    #if PBR
    const vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    const vec3 ambient = albedo.rgb * ao * .08f;
    irradiance += ambient;
    #endif

    for(uint i = 0; i < u_Lights.DirectionalLightCount; ++i)
    {
        DirectionalLight dl = u_Lights.DirectionalLights[i];
        
        const float kShadow = 1.f - ShadowCalculation(u_DirShadowmap[i], u_Lights.DirLightViewProjMatrices[i] * vec4(i_VertexInput.WorldPos, 1), N, normalize(dl.Direction));
        #if PHONG
            irradiance += DirectionalLightContribution(kShadow, V, N, dl, albedo.rgb, ao);
        #elif PBR
            irradiance += DirectionalLightContribution(kShadow, F0, V, N, dl, albedo.rgb, roughness, metallic);
        #endif
    }

    const uint linearTileIndex = GetLinearGridIndex(gl_FragCoord.xy, u_GlobalCameraData.FullResolution.x);
    // Point lights
    {
        const uint offset = linearTileIndex * MAX_POINT_LIGHTS;
        for(uint i = 0; i < u_Lights.PointLightCount && s_VisiblePointLightIndicesBuffer.indices[offset + i] != INVALID_LIGHT_INDEX; ++i) 
        {
            const uint lightIndex = s_VisiblePointLightIndicesBuffer.indices[offset + i];

            PointLight pl = u_Lights.PointLights[lightIndex];

            #if PHONG
                irradiance += PointLightContribution(i_VertexInput.WorldPos, N, V, pl, albedo.rgb, ao);
            #elif PBR
                irradiance += PointLightContribution(i_VertexInput.WorldPos,F0,  N, V, pl, albedo.rgb, roughness, metallic);
            #endif
        }
    }
    
    // Spot lights
    {
        const uint offset = linearTileIndex * MAX_SPOT_LIGHTS;
        for(uint i = 0; i < u_Lights.SpotLightCount && s_VisibleSpotLightIndicesBuffer.indices[offset + i] != INVALID_LIGHT_INDEX; ++i)
        {
           const uint lightIndex = s_VisibleSpotLightIndicesBuffer.indices[offset + i];
           
           SpotLight spl = u_Lights.SpotLights[lightIndex];
           #if PHONG
               irradiance += SpotLightContribution(i_VertexInput.WorldPos, N, V, spl, albedo.rgb, ao);
           #elif PBR
               irradiance += SpotLightContribution(i_VertexInput.WorldPos, F0, N, V, spl, albedo.rgb, roughness, metallic);
           #endif
        }
    }

    // reinhard tone-mapping
    vec3 finalColor = irradiance / (irradiance + vec3(1.0));

    // gamma correction
    finalColor =  pow(finalColor, vec3(1.0 / 2.2));
    
    outFragColor = vec4(finalColor, albedo.a) * i_VertexInput.Color;
}