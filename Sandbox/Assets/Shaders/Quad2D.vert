#version 460

#extension GL_GOOGLE_include_directive : require
#include "Include/Globals.h"

layout(location = 0) out vec2  outUV;
layout(location = 1) out vec4  outColor;
layout(location = 2) out flat uint outTextureIndex; 

const vec3 g_Vertices[6] = {
    vec3(-1.f, -1.f, 0.f),
    vec3(1.f,  -1.f, 0.f),
    vec3(1.f,   1.f, 0.f),  
    vec3(1.f,   1.f, 0.f),
    vec3(-1.f,  1.f, 0.f),
    vec3(-1.f, -1.f, 0.f)
};

const vec2 g_TexCoords[6] = {
	vec2( 0.f, 0.f ),
	vec2( 1.f, 0.f ),
	vec2( 1.f, 1.f ),
	vec2( 1.f, 1.f ),
	vec2( 0.f, 1.f ),
	vec2( 0.f, 0.f )
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer QuadDrawBuffer
{
    Sprite sprites[];
} s_QuadDrawBufferBDA;

void main()
{
    Sprite sprite = QuadDrawBuffer(u_PC.addr0).sprites[gl_InstanceIndex];

    const vec3 worldPos = RotateByQuat(sprite.Scale * g_Vertices[gl_VertexIndex], sprite.Orientation) + sprite.Translation;
    gl_Position = CameraData(u_PC.CameraDataBuffer).ViewProjection * vec4(worldPos, 1.0);
    
    outUV = g_TexCoords[gl_VertexIndex];
    outColor = glm_unpackUnorm4x8(sprite.Color);
    outTextureIndex = sprite.BindlessTextureIndex;
}
