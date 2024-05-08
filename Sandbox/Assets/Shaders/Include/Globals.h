#ifdef __cplusplus
#pragma once

#include "Meshlets.h"
#include "Lights.h"
#include "Primitives.h"

using vec2    = glm::vec2;
using u16vec2 = glm::u16vec2;
using u8vec4  = glm::u8vec4;
using vec3    = glm::vec3;
using vec4    = glm::vec4;
using mat4    = glm::mat4;

// TODO: Make bindless renderer somehow automated for creation from shader headers files
static constexpr uint32_t s_MAX_TEXTURES        = BIT(16);
static constexpr uint32_t s_MAX_IMAGES          = BIT(16);
static constexpr uint32_t s_MAX_STORAGE_BUFFERS = BIT(16);
static constexpr uint32_t s_MAX_RENDER_OBJECTS  = BIT(16);

#else

// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require  // GL_EXT_buffer_reference extension is also implicitly enabled.

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// NOTE: Mark things as Global/BDA to make sure that basic shader reflection won't catch it!

#include "Include/Meshlets.h"
#include "Include/Lights.h"
#include "Include/Primitives.h"

#endif

#ifndef SHADER_DEBUG_PRINTF
#define SHADER_DEBUG_PRINTF 0
#endif

#ifndef __cplusplus

#if SHADER_DEBUG_PRINTF
#extension GL_EXT_debug_printf : require
#endif

#endif

#define SSS_LOCAL_GROUP_SIZE 16u

struct MeshPositionVertex
{
    vec3 Position;
};

struct MeshAttributeVertex
{
    u8vec4 Color;
    vec3 Normal;
    vec3 Tangent;
#ifdef __cplusplus
    u16vec2 UV;
#else
    f16vec2 UV;
#endif
};

#ifdef __cplusplus
static Plane ComputePlane(const glm::vec3& p0, const glm::vec3& normal)
{
    Plane plane;
    plane.Normal   = glm::normalize(normal);
    plane.Distance = glm::dot(p0, plane.Normal);  // signed distance to the origin using p0

    return plane;
}

#else
vec4 UnpackU8Vec4ToVec4(u8vec4 packed)
{
    const float invDivisor = 1 / 255.0f;
    const float r          = invDivisor * packed.r;
    const float g          = invDivisor * packed.g;
    const float b          = invDivisor * packed.b;
    const float a          = invDivisor * packed.a;

    return vec4(r, g, b, a);
}

// From glm
vec3 RotateByQuat(const vec3 position, const vec4 orientation)
{
    const vec3 uv = cross(orientation.xyz, position);

    // NOTE: That the center of rotation is always the origin, so we adding an offset(position).
    return position + (uv * orientation.w + cross(orientation.xyz, uv)) * 2.0;
}

#endif

const uint32_t s_INVALID_CULLED_OBJECT_INDEX = 2 << 20;

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

struct MeshData
{
    vec3 translation;
    vec3 scale;
    vec4 orientation;
    Sphere sphere;
    uint32_t vertexPosBufferIndex;
    uint32_t vertexAttributeBufferIndex;
    uint32_t indexBufferIndex;
    uint32_t meshletVerticesBufferIndex;
    uint32_t meshletTrianglesBufferIndex;
    uint32_t meshletBufferIndex;
    uint32_t materialBufferIndex;
};

const uint32_t BINDLESS_MEGA_SET = 0;

const uint32_t TEXTURE_BINDING       = 0;
const uint32_t STORAGE_IMAGE_BINDING = 1;

const uint32_t STORAGE_BUFFER_VERTEX_POS_BINDING            = 2;
const uint32_t STORAGE_BUFFER_VERTEX_ATTRIB_BINDING         = 3;
const uint32_t STORAGE_BUFFER_MESHLET_BINDING               = 4;
const uint32_t STORAGE_BUFFER_MESHLET_VERTEX_BINDING        = 5;
const uint32_t STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING      = 6;
const uint32_t STORAGE_BUFFER_MESH_MATERIAL_BINDING         = 7;
const uint32_t STORAGE_BUFFER_INDEX_BINDING                 = 8;
const uint32_t STORAGE_BUFFER_MESH_DATA_OPAQUE_BINDING      = 9;
const uint32_t STORAGE_BUFFER_MESH_DATA_TRANSPARENT_BINDING = 10;

// NOTE: I'll have to offset manually in other shaders from this set.
const uint32_t LAST_BINDLESS_SET = BINDLESS_MEGA_SET;

#ifndef __cplusplus

layout(set = BINDLESS_MEGA_SET, binding = TEXTURE_BINDING) uniform sampler2D u_GlobalTextures[];
layout(set = BINDLESS_MEGA_SET, binding = TEXTURE_BINDING) uniform usampler2D u_GlobalTextures_uint[];
layout(set = BINDLESS_MEGA_SET, binding = TEXTURE_BINDING) uniform sampler3D u_GlobalTextures3D[];
layout(set = BINDLESS_MEGA_SET, binding = TEXTURE_BINDING) uniform usampler3D u_GlobalTextures3D_uint[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_IMAGE_BINDING, rgba8) uniform image2D u_GlobalImages_RGBA8[];
layout(set = BINDLESS_MEGA_SET, binding = STORAGE_IMAGE_BINDING, rgba16f) uniform image2D u_GlobalImages_RGBA16F[];
layout(set = BINDLESS_MEGA_SET, binding = STORAGE_IMAGE_BINDING, rgba32f) uniform image2D u_GlobalImages_RGBA32F[];
layout(set = BINDLESS_MEGA_SET, binding = STORAGE_IMAGE_BINDING, r32f) uniform image2D u_GlobalImages_R32F[];

// NOTE: Every submesh has it's own storage buffer, where stored array of Positions, etc...

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_VERTEX_POS_BINDING, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex Positions[];
}
s_GlobalVertexPosBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex Attributes[];
}
s_GlobalVertexAttribBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESHLET_BINDING, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
}
s_GlobalMeshletBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESHLET_VERTEX_BINDING, scalar) readonly buffer MeshletVerticesBuffer
{
    uint32_t vertices[];
}
s_GlobalMeshletVerticesBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING, scalar) readonly buffer MeshletTrianglesBuffer
{
    uint8_t triangles[];
}
s_GlobalMeshletTrianglesBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESH_MATERIAL_BINDING, scalar) readonly buffer MaterialBuffer
{
    PBRData mat;
}
s_GlobalMaterialBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_INDEX_BINDING, scalar) readonly buffer MeshIndexBuffer
{
    uint32_t indices[];
}
s_GlobalIndexBuffers[];

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESH_DATA_OPAQUE_BINDING, scalar) readonly buffer MeshDataBufferOpaque
{
    MeshData MeshesData[];
}
s_GlobalMeshDataBufferOpaque;

layout(set = BINDLESS_MEGA_SET, binding = STORAGE_BUFFER_MESH_DATA_TRANSPARENT_BINDING, scalar) readonly buffer MeshDataBufferTransparent
{
    MeshData MeshesData[];
}
s_GlobalMeshDataBufferTransparent;

#endif

#ifdef __cplusplus
struct CameraData
#else
layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer CameraData
#endif
{
    Frustum ViewFrustum;
    mat4 Projection;
    mat4 View;
    mat4 ViewProjection;
    mat4 InverseProjection;
    vec3 Position;
    float zNear;
    float zFar;
    float FOV;
    vec2 FullResolution;
    vec2 InvFullResolution;
}
#ifdef __cplusplus
;
#else
s_CameraDataBDA;  // Name unused, check u_PC
#endif

#ifdef __cplusplus

struct alignas(16) LightData
#else
layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer LightData
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
s_LightsBDA;  // Name unused, check u_PC
#endif

#ifndef __cplusplus

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer LightCullingFrustum
{
    TileFrustum frustums[];
}
s_GridFrustumsBufferBDA;  // Name unused, check u_PC

layout(buffer_reference, buffer_reference_align = 1, scalar) buffer VisiblePointLightIndicesBuffer
{
    LIGHT_INDEX_TYPE indices[];
}
s_VisiblePointLightIndicesBufferBDA;  // Name unused, check u_PC

layout(buffer_reference, buffer_reference_align = 1, scalar) buffer VisibleSpotLightIndicesBuffer
{
    LIGHT_INDEX_TYPE indices[];
}
s_VisibleSpotLightIndicesBufferBDA;  // Name unused, check u_PC

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
    uint32_t MaterialBufferIndex;

    uint64_t LightCullingFrustumDataBuffer;
    uint64_t VisiblePointLightIndicesDataBuffer;
    uint64_t VisibleSpotLightIndicesDataBuffer;

#ifdef __cplusplus
    uint64_t LightDataBuffer;
#else
    LightData LightDataBuffer;
#endif

#ifdef __cplusplus
    uint64_t CameraDataBuffer;
#else
    CameraData CameraDataBuffer;
#endif
    vec2 pad0;
}
#ifdef __cplusplus
;
#else
u_PC;
#endif

#ifndef __cplusplus
vec4 ClipSpaceToView(const vec4 clip)
{
    vec4 view = u_PC.CameraDataBuffer.InverseProjection * clip;
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
vec4 ScreenSpaceToView(const vec4 screen, const vec2 inverseScreenDimensions)
{
    const vec2 uv = screen.xy * inverseScreenDimensions;  // convert from range [0, width]-[0, height] to [0, 1], [0, 1]

    /* If screen origin is top left like in DX or Vulkan: (uv.x, 1.0f - uv.y), screen.z - depth in range [0, 1] like in DX or Vulkan*/
    const vec4 clip = vec4(vec2(uv.x, 1.0 - uv.y) * 2.0 - 1.0, screen.z,  // doesn't affect vulkan
                           screen.w);  // convert from [0, 1] to NDC([-1, 1]), without touching depth since it's [0, 1] as I require.
    return ClipSpaceToView(clip);
}

vec4 ViewToScreenSpace(const vec4 viewPos)
{
    vec4 ss = u_PC.CameraDataBuffer.Projection * viewPos;
    ss      = ss / ss.w;
    ss.xy   = vec2(ss.x, 1.f - ss.y) * .5f + .5f;
    ss      = vec4(ss.xy * u_PC.CameraDataBuffer.InvFullResolution, ss.z, ss.w);

    return ss;
}

// NOTE: A bit faster than inverse matrix mul.
// XeGTAO uses same thing, also better explanation is right there:
// https://www.youtube.com/watch?v=z1KG2Cwi1pk&list=PLU2nPsAdxKWQYxkmQ3TdbLsyc1l2j25XM&index=125&ab_channel=GameEngineSeries
float ScreenSpaceDepthToView(const float fScreenDepth)
{
    return -u_PC.CameraDataBuffer.Projection[3][2] / (fScreenDepth + u_PC.CameraDataBuffer.Projection[2][2]);
}

// https://developer.download.nvidia.com/cg/saturate.html
float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

#endif
