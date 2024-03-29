#ifdef __cplusplus
#pragma once

#include "Meshlets.h"
#include "Lights.h"

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;

// TODO: Make bindless renderer somehow automated for creation from shader headers files
static constexpr uint32_t s_MAX_TEXTURES        = BIT(16);
static constexpr uint32_t s_MAX_IMAGES          = BIT(16);
static constexpr uint32_t s_MAX_STORAGE_BUFFERS = BIT(16);

#else

// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// NOTE: Mark things as Global to make sure that basic shader reflection won't catch it!

// MESHLETS
#include "Assets/Shaders/Include/Meshlets.h"
#include "Assets/Shaders/Include/Lights.h"

#endif

#define SHADER_DEBUG_PRINTF 0

#ifndef __cplusplus

#if SHADER_DEBUG_PRINTF
#extension GL_EXT_debug_printf : require
#endif

#endif

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
    vec3 Normal;  // Should be normalized.
    // LearnOpenGL:  distance from origin to the nearest point in the plane(PERPENDICULAR VECTOR FROM THE ORIGIN IN OTHER WORDS)
    float Distance;  // Signed distance from the plane to the origin of the world(or whatever coordinate space you're working in).
};

#ifdef __cplusplus
static Plane ComputePlane(const glm::vec3& p0, const glm::vec3& normal)
{
    Plane plane;
    plane.Normal   = glm::normalize(normal);
    plane.Distance = glm::dot(plane.Normal, p0);  // signed distance to the origin using p0

    return plane;
}
#endif

#ifndef __cplusplus
Plane ComputePlane(const vec3 p0, const vec3 p1, const vec3 p2)
{
    Plane plane;

    plane.Normal   = normalize(cross(p1 - p0, p2 - p0));
    plane.Distance = dot(plane.Normal, p0);  // signed distance to the origin using p0

    return plane;
}
#endif

struct AABB
{
    vec3 Center;
    vec3 Extents;
};

// Left, Right, Top, Bottom, Near, Far
struct Frustum
{
    Plane Planes[6];
};

struct Sphere
{
    vec3 Center;
    float Radius;
};

struct PBRData
{
    vec4 BaseColor;
    float Roughness;
    float Metallic;
    uint32_t AlbedoTextureIndex;
    uint32_t NormalTextureIndex;
    uint32_t MetallicRoughnessTextureIndex;
    uint32_t EmissiveTextureIndex;
    uint32_t OcclusionTextureIndex;
    bool bIsOpaque;
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
const uint32_t STORAGE_BUFFER_MESH_MATERIAL_BINDING    = 5;
const uint32_t STORAGE_BUFFER_INDEX_BUFFER_BINDING     = 6;

// Frame data set
const uint32_t FRAME_DATA_BUFFER_SET = 2;

const uint32_t FRAME_DATA_BUFFER_CAMERA_BINDING = 0;
const uint32_t FRAME_DATA_BUFFER_LIGHTS_BINDING = 1;

// NOTE: I'll have to offset manually in other shaders from this set.
const uint32_t LAST_BINDLESS_SET = FRAME_DATA_BUFFER_SET;

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

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_MESH_MATERIAL_BINDING, scalar) readonly buffer MaterialBuffer
{
    PBRData mat;
}
s_GlobalMaterialBuffers[];

layout(set = STORAGE_BUFFER_SET, binding = STORAGE_BUFFER_INDEX_BUFFER_BINDING, scalar) readonly buffer MeshIndexBuffer
{
    uint32_t indices[];
}
s_GlobalIndexBuffers[];

#endif

#ifdef __cplusplus
struct CameraData
#else
layout(set = FRAME_DATA_BUFFER_SET, binding = FRAME_DATA_BUFFER_CAMERA_BINDING, scalar) uniform CameraUB
#endif
{
    mat4 Projection;
    mat4 View;
    mat4 ViewProjection;
    mat4 InverseProjection;
    vec3 Position;
    float zNear;
    float zFar;
    float FOV;
    vec2 FullResolution;
}
#ifdef __cplusplus
;
#else
u_GlobalCameraData;
#endif

#ifndef __cplusplus
vec4 ClipSpaceToView(const vec4 clip)
{
    vec4 view = u_GlobalCameraData.InverseProjection * clip;
    return view / view.w;
}

/* RHI and their NDCs:
    OpenGL, OpenGL ES and WebGL NDC: +Y is up. Point(-1, -1) is at the bottom left corner.
    Framebuffer coordinate: +Y is up.Origin(0, 0) is at the bottom left corner.
    Texture coordinate:     +Y is up.Origin(0, 0) is at the bottom left corner.
    See OpenGL 4.6 spec, Figure 8.4.

    D3D12 and Metal NDC:    +Y is up. Point(-1, -1) is at the bottom left corner
    Framebuffer coordinate: +Y is down. Origin(0, 0) is at the top left corner
    Texture coordinate:     +Y is down. Origin(0, 0) is at the top left corner.

    Vulkan NDC: +Y is down. Point(-1, -1) is at the top left corner.
    Framebuffer coordinate: +Y is down. Origin(0, 0) is at the top left corner.
        See the description about “VkViewport” and “FragCoord” in Vulkan 1.1 spec.
        But we can flip the viewport coordinate via a negative viewport height value. NOTE!!!: Works only via graphics pipelines!
    Texture coordinate:     +Y is down. Origin(0, 0) is at the top left corner.
*/
vec4 ScreenSpaceToView(const vec4 screen, const vec2 screenDimensions)
{
    const vec2 uv = screen.xy / screenDimensions;  // convert from range [0, width]-[0, height] to [0, 1], [0, 1]

    /* If screen origin is top left like in DX or Vulkan: (uv.x, 1.0f - uv.y), screen.z - depth in range [0, 1] like in DX or Vulkan*/
    const vec4 clip = vec4(vec2(uv.x, 1.0 - uv.y) * 2.0 - 1.0, screen.z,
                           screen.w);  // convert from [0, 1] to NDC([-1, 1]), without touching depth since it's [0, 1] as I require.
    return ClipSpaceToView(clip);
}

#endif

const uint32_t MAX_SHADOW_CASCADES = 5;

#ifdef __cplusplus
struct LightsData
#else
layout(set = FRAME_DATA_BUFFER_SET, binding = FRAME_DATA_BUFFER_LIGHTS_BINDING, scalar) readonly buffer LightsSB
#endif
{
    PointLight PointLights[MAX_POINT_LIGHTS];
    mat4 PointLightViewProjMatrices[MAX_POINT_LIGHTS * 6];
    SpotLight SpotLights[MAX_SPOT_LIGHTS];
    DirectionalLight DirectionalLights[MAX_DIR_LIGHTS];
    mat4 DirLightViewProjMatrices[MAX_DIR_LIGHTS * MAX_SHADOW_CASCADES];
    float CascadePlaneDistances[MAX_SHADOW_CASCADES - 1];
    uint32_t PointLightCount;
    uint32_t SpotLightCount;
    uint32_t DirectionalLightCount;
}
#ifdef __cplusplus
;
#else
u_Lights;
#endif

#ifdef __cplusplus
struct PushConstantBlock
{
#else
layout(push_constant, scalar) uniform PushConstantBlock
{
#endif
    mat4 Transform;
    uint32_t StorageImageIndex;
    uint32_t AlbedoTextureIndex;

    uint32_t MeshIndexBufferIndex;
    uint32_t VertexPosBufferIndex;
    uint32_t VertexAttribBufferIndex;

    uint32_t MeshletBufferIndex;
    uint32_t MeshletVerticesBufferIndex;
    uint32_t MeshletTrianglesBufferIndex;

    uint32_t MaterialBufferIndex;

    vec4 pad0;
    vec3 pad1;
}
#ifdef __cplusplus
;
#else
u_PC;
#endif