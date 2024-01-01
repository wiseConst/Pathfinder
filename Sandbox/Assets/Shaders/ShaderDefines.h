

// Bindless https://vincent-p.github.io/posts/vulkan_bindless_descriptors/

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

// TODO: Should be global for all shaders
layout(set = 0, binding = 0) uniform sampler2D u_GlobalTextures[];  // combined image samplers

// layout(set = 1, binding = 0) uniform sampler2D global_textures[];
// layout(set = 1, binding = 0) uniform usampler2D global_textures_uint[];
// layout(set = 1, binding = 0) uniform sampler3D global_textures_3d[];
// layout(set = 1, binding = 0) uniform usampler3D global_textures_3d_uint[];

layout(set = 1, binding = 0, rgba8) uniform image2D u_GlobalStorageImages[];  // storage image samplers

layout(set = 2, binding = 0, scalar) uniform CameraUB
{
    mat4 Projection;
    vec3 Position;
} u_GlobalCameraData;

// layout(set = 2, binding = 0, rgba8) uniform image2D global_images_2d_rgba8[];
// layout(set = 2, binding = 0, rgba16f) uniform image2D global_images_2d_rgba16f[];
// layout(set = 2, binding = 0, rgba32f) uniform image2D global_images_2d_rgba32f[];
// layout(set = 2, binding = 0, r32f) uniform image2D global_images_2d_r32f[];