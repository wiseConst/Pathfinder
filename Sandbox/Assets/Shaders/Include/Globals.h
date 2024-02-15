#ifdef __cplusplus
#pragma once

#include "Meshlets.h"
#include "Lights.h"

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;

#else

// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define DEBUG_PRINTF 1

#if DEBUG_PRINTF
#extension GL_EXT_debug_printf : require
#endif

// NOTE: Mark things as Global to make sure that basic shader reflection won't catch it!

// MESHLETS
#include "Assets/Shaders/Include/Meshlets.h"
#include "Assets/Shaders/Include/Lights.h"

#endif

const uint32_t LIGHT_CULLING_TILE_SIZE = 16u;
const uint32_t INVALID_LIGHT_INDEX     = 1 << 16;

struct MeshPositionVertex
{
    vec3 Position;
};

// TODO: Packing
struct MeshAttributeVertex
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
    vec2 UV;
};

struct Plane
{
    vec3 Normal;     // Should be normalized.
    float Distance;  // Signed distance from the plane to the origin of the world(or whatever coordinate space you're working in).
};

// Left, Right, Top, Bottom, Near, Far
struct Frustum
{
    Plane Planes[6];
};

struct Material
{
    vec4 BaseColor;
    float Metallic;
    float Roughness;
};

// Images/Textures
const uint32_t TEXTURE_STORAGE_IMAGE_SET = 0;

const uint32_t TEXTURE_BINDING       = 0;
const uint32_t STORAGE_IMAGE_BINDING = 1;

// Storage buffers
const uint32_t STORAGE_BUFFER_SET = 1;

const uint32_t STORAGE_BUFFER_VERTEX_POS_BINDING       = 0;
const uint32_t STORAGE_BUFFER_VERTEX_ATTRIB_BINDING    = 1;
const uint32_t STORAGE_BUFFER_MESHLET_BINDING          = 2;
const uint32_t STORAGE_BUFFER_MESHLET_VERTEX_BINDING   = 3;
const uint32_t STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING = 4;

// Uniform buffers
const uint32_t UNIFORM_BUFFER_SET = 2;

const uint32_t UNIFORM_BUFFER_CAMERA_BINDING = 0;
const uint32_t UNIFORM_BUFFER_LIGHTS_BINDING = 1;

// NOTE: I'll have to offset manually in other shaders from this set.
const uint32_t LAST_BINDLESS_SET = UNIFORM_BUFFER_SET;

#ifdef __cplusplus
#else

layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = TEXTURE_BINDING) uniform sampler2D u_GlobalTextures[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = TEXTURE_BINDING) uniform usampler2D u_GlobalTextures_uint[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = TEXTURE_BINDING) uniform sampler3D u_GlobalTextures3D[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = TEXTURE_BINDING) uniform usampler3D u_GlobalTextures3D_uint[];

layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = STORAGE_IMAGE_BINDING, rgba8) uniform image2D u_GlobalImages_RGBA8[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = STORAGE_IMAGE_BINDING, rgba16f) uniform image2D u_GlobalImages_RGBA16F[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = STORAGE_IMAGE_BINDING, rgba32f) uniform image2D u_GlobalImages_RGBA32F[];
layout(set = TEXTURE_STORAGE_IMAGE_SET, binding = STORAGE_IMAGE_BINDING, r32f) uniform image2D u_GlobalImages_R32F[];

// NOTE: Every submesh has it's own storage buffer, where stored array of Positions, etc...

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_VERTEX_POS_BINDING, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex Positions[];
}
s_GlobalVertexPosBuffers[];

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex Attributes[];
}
s_GlobalVertexAttribBuffers[];

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_MESHLET_BINDING, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
}
s_GlobalMeshletBuffers[];

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_MESHLET_VERTEX_BINDING, scalar) readonly buffer MeshletVerticesBuffer
{
    uint32_t vertices[];
}
s_GlobalMeshletVerticesBuffers[];

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING, scalar) readonly buffer MeshletTrianglesBuffer
{
    uint8_t triangles[];
}
s_GlobalMeshletTrianglesBuffers[];

/*
layout(set = 2, binding = STORAGE_BUFFER_MESH_MATERIAL_BINDING, scalar) readonly buffer MaterialBuffer
{
    Material mat;
} s_GlobalMaterialBuffers[];
*/

#endif

#ifdef __cplusplus
struct CameraData
#else
layout(set = UNIFORM_BUFFER_SET, binding = UNIFORM_BUFFER_CAMERA_BINDING, scalar) uniform CameraUB
#endif
{
    mat4 Projection;
    mat4 InverseView;
    mat4 ViewProjection;
    mat4 InverseProjection;
    vec3 Position;
    float zNear;
    float zFar;
    vec2 FramebufferResolution;
}
#ifdef __cplusplus
;
#else
u_GlobalCameraData;
#endif

#ifdef __cplusplus
struct LightsData
#else
layout(set = UNIFORM_BUFFER_SET, binding = UNIFORM_BUFFER_LIGHTS_BINDING, scalar) uniform LightsUB
#endif
{
    PointLight PointLights[MAX_POINT_LIGHTS];
    SpotLight SpotLights[MAX_SPOT_LIGHTS];
    DirectionalLight DirectionalLights[MAX_DIR_LIGHTS];
    uint32_t PointLightCount;
    uint32_t SpotLightCount;
    uint32_t DirectionalLightCount;
}
#ifdef __cplusplus
;
#else
u_Lights;
#endif

// TODO: Add material index
#ifdef __cplusplus
struct PushConstantBlock
{
    glm::mat4 Transform = glm::mat4(1.0f);  // TODO: Remove with glm define GLM_FORCE_CTOR_INIT ??
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