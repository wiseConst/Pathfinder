// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#ifdef __cplusplus
#pragma once

#include "Meshlets.h"

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

#else

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define DEBUG_PRINTF 0

#if DEBUG_PRINTF
#extension GL_EXT_debug_printf : require
#endif

// TODO: 0 index image/texture should be white or black(default)
// NOTE: Mark things as Global to make sure that basic shader reflection won't catch it!

// combined image samplers (textures)
layout(set = 0, binding = 0) uniform sampler2D u_GlobalTextures[]; 
//layout(set = 0, binding = 0) uniform usampler2D u_GlobalTextures_uint[];
//layout(set = 0, binding = 0) uniform sampler3D u_GlobalTextures3D[];
//layout(set = 0, binding = 0) uniform usampler3D u_GlobalTextures3D_uint[];

// storage images
layout(set = 1, binding = 0, rgba8)   uniform image2D u_GlobalImages_RGBA8[]; 
layout(set = 1, binding = 0, rgba16f) uniform image2D u_GlobalImages_RGBA16F[];
layout(set = 1, binding = 0, rgba32f) uniform image2D u_GlobalImages_RGBA32F[];
layout(set = 1, binding = 0, r32f)    uniform image2D u_GlobalImages_R32F[];

// MESHLETS
#include "Assets/Shaders/Include/Meshlets.h"

#endif

struct MeshPositionVertex
{
    vec3 Position;
};

struct MeshAttributeVertex
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
    vec2 UV;
};

struct Frustum
{
    vec3 Bottom;
    vec3 Top;
    vec3 Left;
    vec3 Right;
    vec3 Near;
    vec3 Far; 
};

struct Material
{
    vec4 BaseColor;
    float Metallic;
    float Roughness;
};

const uint32_t STORAGE_BUFFER_VERTEX_POS_BINDING = 0;
const uint32_t STORAGE_BUFFER_VERTEX_ATTRIB_BINDING = 1;
const uint32_t STORAGE_BUFFER_MESHLET_BINDING = 2;
const uint32_t STORAGE_BUFFER_MESHLET_VERTEX_BINDING = 3;
const uint32_t STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING = 4;

#ifdef __cplusplus
#else

// NOTE: Every submesh has it's own storage buffer, where stored array of Positions, etc...

layout(set = 2, binding = STORAGE_BUFFER_VERTEX_POS_BINDING, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex Positions[];
}
s_GlobalVertexPosBuffers[];

layout(set = 2, binding = STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex Attributes[];
}
s_GlobalVertexAttribBuffers[];

layout(set = 2, binding = STORAGE_BUFFER_MESHLET_BINDING, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
}
s_GlobalMeshletBuffers[];

layout(set = 2, binding = STORAGE_BUFFER_MESHLET_VERTEX_BINDING, scalar) readonly buffer MeshletVerticesBuffer
{
    uint32_t vertices[];
}
s_GlobalMeshletVerticesBuffers[];

layout(set = 2, binding = STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING, scalar) readonly buffer MeshletTrianglesBuffer
{
    uint8_t triangles[];
}
s_GlobalMeshletTrianglesBuffers[];

// CAMERA
layout(set = 3, binding = 0, scalar) uniform CameraUB
{
    mat4 Projection;
    mat4 InverseView;
    vec3 Position;
    float zNear;
    float zFar;
}
u_GlobalCameraData;

/*
layout(set = 4, binding = 0, scalar) readonly buffer MaterialBuffer
{
    Material mat;
} s_GlobalMaterialBuffers[];
*/

#endif

#ifdef __cplusplus
struct PushConstantBlock
{
    glm::mat4 Transform = glm::mat4(1.0f); // TODO: Remove with glm define GLM_FORCE_CTOR_INIT ??
#else
layout(push_constant, scalar) uniform PushConstantBlock
{
    mat4 Transform;
#endif
    uint32_t StorageImageIndex;
    
    uint32_t AlbedoTextureIndex;
    uint32_t NormalTextureIndex;
    uint32_t MetallicRoughnessTextureIndex;
    
    uint32_t VertexPosBufferIndex;
    uint32_t VertexAttribBufferIndex;
    
    uint32_t MeshletBufferIndex;
    uint32_t MeshletVerticesBufferIndex;
    uint32_t MeshletTrianglesBufferIndex;
    
    vec4 pad0;
    vec3 pad1;
}
#ifdef __cplusplus
;
#else
u_PC;
#endif