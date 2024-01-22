// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/


#ifdef __cplusplus
#pragma once

#include "Meshlets.h"

#else

#extension GL_EXT_buffer_reference : require

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define DEBUG_PRINTF 0

// TODO: 0 index image/texture should be white or black(default)
// NOTE: Mark things as Global to make sure that basic shader reflection won't catch it!

layout(set = 0, binding = 0) uniform sampler2D u_GlobalTextures[];  // combined image samplers
layout(set = 0, binding = 0) uniform usampler2D u_GlobalTextures_uint[];
layout(set = 0, binding = 0) uniform sampler3D u_GlobalTextures3D[];
layout(set = 0, binding = 0) uniform usampler3D u_GlobalTextures3D_uint[];

layout(set = 1, binding = 0, rgba8) uniform image2D u_GlobalImages_RGBA8[];  // storage images
layout(set = 1, binding = 0, rgba16f) uniform image2D u_GlobalImages_RGBA16F[];
layout(set = 1, binding = 0, rgba32f) uniform image2D u_GlobalImages_RGBA32F[];
layout(set = 1, binding = 0, r32f) uniform image2D u_GlobalImages_R32F[];

// MESHLETS
#include "Assets/Shaders/Include/Meshlets.h"

#endif

struct MeshPositionVertex
{
#ifdef __cplusplus
    glm::vec3 Position;
#else
    vec3 Position;
#endif
};

struct MeshAttributeVertex
{
#ifdef __cplusplus
    glm::vec4 Color;
    glm::vec3 Normal;
    glm::vec3 Tangent;
#else
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
#endif
};

#ifdef __cplusplus
#else

layout(set = 2, binding = 0, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex Positions[];
} s_GlobalVertexPosBuffers[];

layout(set = 2, binding = 1, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex Attributes[];
} s_GlobalVertexAttribBuffers[];

layout(set = 2, binding = 2, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
} s_GlobalMeshletBuffers[];

// CAMERA
layout(set = 3, binding = 0, scalar) uniform CameraUB
{
    mat4 Projection;
    mat4 InverseView;
    vec3 Position;
}
u_GlobalCameraData;

layout(push_constant, scalar) uniform PushConstantBlock
{
    mat4 Transform;
    uint32_t StorageImageIndex;
    uint32_t AlbedoTextureIndex;
    uint32_t NormalTextureIndex;
    uint32_t VertexPosBufferIndex;
    uint32_t VertexAttribBufferIndex;
    uint32_t MeshletBufferIndex;
}
u_PC;

#endif
