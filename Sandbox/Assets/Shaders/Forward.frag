#version 460

#extension GL_GOOGLE_include_directive : require
#include "Assets/Shaders/Include/Globals.h"
#include "Assets/Shaders/Include/PhongShading.glslh"

layout(set = LAST_BINDLESS_SET + 1, binding = 0, scalar) buffer readonly VisiblePointLightIndicesBuffer
{
    uint32_t indices[];
} s_VisiblePointLightIndicesBuffer;
layout(set = LAST_BINDLESS_SET + 1, binding = 1) uniform sampler2D u_SSAO;

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec2 UV;
    vec3 WorldPos;
    mat3 TBNtoWorld;
} i_VertexInput;

void main()
{ 
    const vec3 normalMap = texture(u_GlobalTextures[nonuniformEXT(u_PC.NormalTextureIndex)], i_VertexInput.UV).xyz  * 2.0 - 1.0;
    const vec3 N = normalize(i_VertexInput.TBNtoWorld * normalMap);

    const vec4 albedo = texture(u_GlobalTextures[nonuniformEXT(u_PC.AlbedoTextureIndex)], i_VertexInput.UV);

    const vec2 screenSpaceUV = gl_FragCoord.xy / textureSize(u_SSAO, 0).xy;
    const float ao = texture(u_SSAO, screenSpaceUV).r;
 
    vec3 lightSources = vec3(0);
    for(uint i = 0; i < u_Lights.DirectionalLightCount; ++i)
        lightSources += CalcDirLights(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, u_Lights.DirectionalLights[i], albedo.xyz, ao);
        
    const uint tileIndex = uint(gl_FragCoord.x) / LIGHT_CULLING_TILE_SIZE;
    const uint offset = tileIndex * MAX_POINT_LIGHTS;
    
    for(uint i = 0; i < u_Lights.PointLightCount; ++i) 
    {
        const uint lightIndex = s_VisiblePointLightIndicesBuffer.indices[offset + i];
        if(lightIndex == INVALID_LIGHT_INDEX || lightIndex >= u_Lights.PointLightCount) continue;

        PointLight pl = u_Lights.PointLights[lightIndex];
        lightSources += CalcPointLights(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, pl, albedo.xyz, ao);
    }
    
    for(uint i = 0; i < u_Lights.SpotLightCount; ++i)
        lightSources += CalcSpotLights(i_VertexInput.WorldPos, N, u_GlobalCameraData.Position, u_Lights.SpotLights[i]);
    
    outFragColor = vec4(lightSources, 1) * i_VertexInput.Color;
}