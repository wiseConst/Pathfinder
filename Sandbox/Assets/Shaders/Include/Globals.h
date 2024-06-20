#ifdef __cplusplus
#pragma once

#include "Meshlets.h"
#include "Lights.h"
#include "Primitives.h"

static constexpr uint32_t s_MAX_TEXTURES = BIT(20);
static constexpr uint32_t s_MAX_IMAGES   = BIT(20);

#else

#include "Pathfinder.glsl"
#include "Include/Meshlets.h"
#include "Include/Lights.h"
#include "Include/Primitives.h"
#include "Include/Math.glsl"
#include "Utils/Branching.glsl"

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

struct Sprite
{
    vec3 Translation;
    vec3 Scale;
    vec4 Orientation;
    uint32_t Color;
    uint32_t BindlessTextureIndex;
};

struct MeshPositionVertex
{
    vec3 Position;
};

struct MeshAttributeVertex
{
    uint32_t Color;
    u8vec3 Normal;
    u8vec3 Tangent;  // NOTE: TODO: Why would I use 4th component of Tangent? flipping the normals or what?
#ifdef __cplusplus
    u16vec2 UV;
#else
    f16vec2 UV;
#endif
};

const uint32_t s_INVALID_CULLED_OBJECT_INDEX = 1 << 21;

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
    Sphere sphere;
    uint32_t meshletCount;
    vec3 translation;
    vec3 scale;
    vec4 orientation;
    uint64_t materialBufferBDA;
    uint64_t indexBufferBDA;
    uint64_t vertexPosBufferBDA;
    uint64_t vertexAttributeBufferBDA;
    uint64_t meshletBufferBDA;
    uint64_t meshletVerticesBufferBDA;
    uint64_t meshletTrianglesBufferBDA;
};

const uint32_t BINDLESS_MEGA_SET = 0;

const uint32_t TEXTURE_BINDING       = 0;
const uint32_t STORAGE_IMAGE_BINDING = 1;

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

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex positions[];
}
s_VertexPosBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex attributes[];
}
s_VertexAttribBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshIndexBuffer
{
    uint32_t indices[];
}
s_IndexBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
}
s_MeshletBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletVerticesBuffer
{
    uint32_t vertices[];
}
s_MeshletVerticesBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletTrianglesBuffer
{
    uint8_t triangles[];
}
s_MeshletTrianglesBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MaterialBuffer
{
    PBRData mat;
}
s_MaterialBuffersBDA;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshDataBuffer
{
    MeshData meshesData[];
}
s_MeshDataBufferBDA;

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

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer CulledMeshIDBuffer
{
    uint32_t CulledMeshIDs[];
}
s_CulledMeshIDBufferBDA;  // Name unused, check u_PC

#endif

#ifdef __cplusplus
struct PushConstantBlock
{
#else
layout(push_constant, scalar) uniform PushConstantBlock
{
#endif
    uint64_t CameraDataBuffer;
    uint64_t LightDataBuffer;
    vec4 data0;
    vec4 data1;
    vec4 data2;
    uint32_t StorageImageIndex;
    uint32_t AlbedoTextureIndex;

    uint64_t LightCullingFrustumDataBuffer;
    uint64_t VisiblePointLightIndicesDataBuffer;
    uint64_t VisibleSpotLightIndicesDataBuffer;
    uint64_t addr0;
    uint64_t addr1;
    uint64_t addr2;
    uint64_t addr3;
}
#ifdef __cplusplus
;
#else
u_PC;
#endif

#ifndef __cplusplus

vec4 ClipSpaceToView(const vec4 clip)
{
    vec4 view = CameraData(u_PC.CameraDataBuffer).InverseProjection * clip;
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
        See the description about �VkViewport� and �FragCoord� in Vulkan 1.1 spec.
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
    vec4 ss = CameraData(u_PC.CameraDataBuffer).Projection * viewPos;
    ss      = ss / ss.w;
    ss.xy   = vec2(ss.x, 1.f - ss.y) * .5f + .5f;
    ss      = vec4(ss.xy * CameraData(u_PC.CameraDataBuffer).InvFullResolution, ss.z, ss.w);

    return ss;
}

// NOTE: A bit faster than inverse matrix mul.
// XeGTAO uses same thing, also better explanation is right there:
// https://www.youtube.com/watch?v=z1KG2Cwi1pk&list=PLU2nPsAdxKWQYxkmQ3TdbLsyc1l2j25XM&index=125&ab_channel=GameEngineSeries
float ScreenSpaceDepthToView(const float fScreenDepth)
{
    return -CameraData(u_PC.CameraDataBuffer).Projection[3][2] / (fScreenDepth + CameraData(u_PC.CameraDataBuffer).Projection[2][2]);
}

#endif