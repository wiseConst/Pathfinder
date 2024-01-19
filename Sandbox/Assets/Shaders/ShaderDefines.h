

// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define DEBUG_PRINTF 0

// TODO: Should be global for all shaders
layout(set = 0, binding = 0) uniform sampler2D u_GlobalTextures[];  // combined image samplers
layout(set = 0, binding = 0) uniform usampler2D u_GlobalTextures_uint[];
layout(set = 0, binding = 0) uniform sampler3D u_GlobalTextures3D[];
layout(set = 0, binding = 0) uniform usampler3D u_GlobalTextures3D_uint[];

layout(set = 1, binding = 0, rgba8) uniform image2D u_GlobalImages_RGBA8[];  // storage images
layout(set = 1, binding = 0, rgba16f) uniform image2D u_GlobalImages_RGBA16F[];
layout(set = 1, binding = 0, rgba32f) uniform image2D u_GlobalImages_RGBA32F[];
layout(set = 1, binding = 0, r32f) uniform image2D u_GlobalImages_R32F[];

layout(set = 2, binding = 0, scalar) uniform CameraUB
{
    mat4 Projection;
    mat4 InverseView;
    vec3 Position;
}
u_GlobalCameraData;

layout(push_constant, scalar) uniform PushConstantBlock
{
    mat4 Transform;
    uint32_t ImageIndex;
    uint32_t TextureIndex;
}
u_PC;