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
#define SHADOW_CASCADE_COUNT 3

struct ShadowCascadeData
{
    mat4 ViewProj[SHADOW_CASCADE_COUNT];
    uint32_t CascadeTextureIndices[SHADOW_CASCADE_COUNT];
};

// Min/Max Z reductioned through CS for sample distribution shadow maps
struct SDSMData
{
    float MinDepth;
    float MaxDepth;
};

struct CSMData
{
    float SplitLambda;
    float CascadePlacementZ[SHADOW_CASCADE_COUNT];
    ShadowCascadeData CascadeData[MAX_DIR_LIGHTS];
};

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
    u8vec4 Tangent;
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

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer VertexPosBuffer
{
    MeshPositionVertex positions[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer VertexAttribBuffer
{
    MeshAttributeVertex attributes[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshIndexBuffer
{
    uint32_t indices[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletBuffer
{
    Meshlet meshlets[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletVerticesBuffer
{
    uint32_t vertices[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshletTrianglesBuffer
{
    uint8_t triangles[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MaterialBuffer
{
    PBRData mat;
};

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer MeshDataBuffer
{
    MeshData meshesData[];
};

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
};

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
};

#ifndef __cplusplus

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer LightCullingFrustum
{
    TileFrustum frustums[];
};

layout(buffer_reference, buffer_reference_align = 1, scalar) buffer VisiblePointLightIndicesBuffer
{
    LIGHT_INDEX_TYPE indices[];
};

layout(buffer_reference, buffer_reference_align = 1, scalar) buffer VisibleSpotLightIndicesBuffer
{
    LIGHT_INDEX_TYPE indices[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer CulledMeshIDBuffer
{
    uint32_t CulledMeshIDs[];
};

layout(buffer_reference, buffer_reference_align = 4, scalar) buffer CSMDataBuffer
{
    CSMData ShadowMapData;
};

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

// NOTE: Seems like both glm and glsl have different functions for packing/unpacking, cuz unpackUnorm4x8 from glsl doesn't work when I pack
// by glm.
vec4 glm_unpackUnorm4x8(const uint32_t packed)
{
    return vec4((packed >> 0) & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF) *
           0.0039215686274509803921568627451f;
}

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

const uint g_POISSON_DISK_SIZE                         = 64;
const vec2 g_POISSON_DISK_SAMPLES[g_POISSON_DISK_SIZE] = {
    vec2(-0.5119625f, -0.4827938f),  vec2(-0.2171264f, -0.4768726f),   vec2(-0.7552931f, -0.2426507f),  vec2(-0.7136765f, -0.4496614f),
    vec2(-0.5938849f, -0.6895654f),  vec2(-0.3148003f, -0.7047654f),   vec2(-0.42215f, -0.2024607f),    vec2(-0.9466816f, -0.2014508f),
    vec2(-0.8409063f, -0.03465778f), vec2(-0.6517572f, -0.07476326f),  vec2(-0.1041822f, -0.02521214f), vec2(-0.3042712f, -0.02195431f),
    vec2(-0.5082307f, 0.1079806f),   vec2(-0.08429877f, -0.2316298f),  vec2(-0.9879128f, 0.1113683f),   vec2(-0.3859636f, 0.3363545f),
    vec2(-0.1925334f, 0.1787288f),   vec2(0.003256182f, 0.138135f),    vec2(-0.8706837f, 0.3010679f),   vec2(-0.6982038f, 0.1904326f),
    vec2(0.1975043f, 0.2221317f),    vec2(0.1507788f, 0.4204168f),     vec2(0.3514056f, 0.09865579f),   vec2(0.1558783f, -0.08460935f),
    vec2(-0.0684978f, 0.4461993f),   vec2(0.3780522f, 0.3478679f),     vec2(0.3956799f, -0.1469177f),   vec2(0.5838975f, 0.1054943f),
    vec2(0.6155105f, 0.3245716f),    vec2(0.3928624f, -0.4417621f),    vec2(0.1749884f, -0.4202175f),   vec2(0.6813727f, -0.2424808f),
    vec2(-0.6707711f, 0.4912741f),   vec2(0.0005130528f, -0.8058334f), vec2(0.02703013f, -0.6010728f),  vec2(-0.1658188f, -0.9695674f),
    vec2(0.4060591f, -0.7100726f),   vec2(0.7713396f, -0.4713659f),    vec2(0.573212f, -0.51544f),      vec2(-0.3448896f, -0.9046497f),
    vec2(0.1268544f, -0.9874692f),   vec2(0.7418533f, -0.6667366f),    vec2(0.3492522f, 0.5924662f),    vec2(0.5679897f, 0.5343465f),
    vec2(0.5663417f, 0.7708698f),    vec2(0.7375497f, 0.6691415f),     vec2(0.2271994f, -0.6163502f),   vec2(0.2312844f, 0.8725659f),
    vec2(0.4216993f, 0.9002838f),    vec2(0.4262091f, -0.9013284f),    vec2(0.2001408f, -0.808381f),    vec2(0.149394f, 0.6650763f),
    vec2(-0.09640376f, 0.9843736f),  vec2(0.7682328f, -0.07273844f),   vec2(0.04146584f, 0.8313184f),   vec2(0.9705266f, -0.1143304f),
    vec2(0.9670017f, 0.1293385f),    vec2(0.9015037f, -0.3306949f),    vec2(-0.5085648f, 0.7534177f),   vec2(0.9055501f, 0.3758393f),
    vec2(0.7599946f, 0.1809109f),    vec2(-0.2483695f, 0.7942952f),    vec2(-0.4241052f, 0.5581087f),   vec2(-0.1020106f, 0.6724468f),
};

vec2 SamplePoissonDisk(const uint32_t index)
{
    return g_POISSON_DISK_SAMPLES[index % g_POISSON_DISK_SIZE];
}

// NOTE: Hash func from Meshlets.h
uint32_t GetUint32PRNG(const vec3 seed, uint32_t state)
{
    state ^= hash(uint32_t(seed.x));
    state ^= hash(uint32_t(seed.y));
    state ^= hash(uint32_t(seed.z));
    return hash(state);
}

#endif