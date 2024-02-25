#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"
#include "Assets/Shaders/Include/PhongShading.glslh"

layout(set = LAST_BINDLESS_SET + 1, binding = 0, scalar) buffer readonly VisiblePointLightIndicesBuffer
{
    uint32_t indices[];
} s_VisiblePointLightIndicesBuffer;
layout(set = LAST_BINDLESS_SET + 1, binding = 1, scalar) buffer readonly VisibleSpotLightIndicesBuffer
{
    uint32_t indices[];
} s_VisibleSpotLightIndicesBuffer;
layout(set = LAST_BINDLESS_SET + 1, binding = 2) uniform sampler2D u_SSAO;
layout(set = LAST_BINDLESS_SET + 1, binding = 3) uniform sampler2D u_DirShadowmap[MAX_DIR_LIGHTS];

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 FragPosLightSpace[MAX_DIR_LIGHTS]; // Dir shadowmap testing
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    mat3 TBNtoWorld;
} i_VertexInput;

vec3 GetNormalFromNormalMap()
{
    const vec3 normal = texture(u_GlobalTextures[nonuniformEXT(u_PC.NormalTextureIndex)], i_VertexInput.UV).xyz;
    return normal * 2.0 - 1.0;
}

void main()
{ 
    const vec3 N = normalize(i_VertexInput.TBNtoWorld * GetNormalFromNormalMap());

    // <0> is reserved for white texture
    const vec3 emissive = u_PC.EmissiveTextureIndex != 0 ? texture(u_GlobalTextures[nonuniformEXT(u_PC.EmissiveTextureIndex)], i_VertexInput.UV).rgb : vec3(0.f);
    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], i_VertexInput.UV);

    const vec2 screenSpaceUV = gl_FragCoord.xy / textureSize(u_SSAO, 0).xy;
    const float ao = texture(u_SSAO, screenSpaceUV).r;
 
    vec3 lightSources = vec3(0);
    for(uint i = 0; i < u_Lights.DirectionalLightCount; ++i)
        lightSources += CalcDirLights(1.f - ShadowCalculation(u_DirShadowmap[i], i_VertexInput.FragPosLightSpace[i],N,normalize(-u_Lights.DirectionalLights[i].Direction)), i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, u_Lights.DirectionalLights[i], albedo.xyz, ao);

    const uint linearTileIndex = GetLinearGridIndex(gl_FragCoord.xy, u_GlobalCameraData.FramebufferResolution.x);
    // Point lights
    {
        const uint offset = linearTileIndex * MAX_POINT_LIGHTS;
        for(uint i = 0; i < u_Lights.PointLightCount && s_VisiblePointLightIndicesBuffer.indices[offset + i] != INVALID_LIGHT_INDEX; ++i) 
        {
            const uint lightIndex = s_VisiblePointLightIndicesBuffer.indices[offset + i];

            PointLight pl = u_Lights.PointLights[lightIndex];
            lightSources += CalcPointLights(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, pl, albedo.xyz, ao);
        }
    }
    
    // Spot lights
    {
        const uint offset = linearTileIndex * MAX_SPOT_LIGHTS;
        for(uint i = 0; i < u_Lights.SpotLightCount && s_VisibleSpotLightIndicesBuffer.indices[offset + i] != INVALID_LIGHT_INDEX; ++i)
        {
           const uint lightIndex = s_VisibleSpotLightIndicesBuffer.indices[offset + i];

           SpotLight spl = u_Lights.SpotLights[lightIndex];
           lightSources += CalcSpotLights(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, spl);
        }
    }
    
    vec4 fragColor = vec4(lightSources, albedo.a) * i_VertexInput.Color;
    fragColor.rgb += emissive;
    outFragColor = fragColor;
}