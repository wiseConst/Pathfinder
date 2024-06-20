#ifdef __cplusplus
#pragma once
#endif

#ifndef __cplusplus

// https://burtleburtle.net/bob/hash/integer.html
uint32_t hash(uint32_t a)
{
    a += ~(a << 15);
    a ^= (a >> 10);
    a += (a << 3);
    a ^= (a >> 6);
    a += ~(a << 11);
    a ^= (a >> 16);
    return a;
}

// Since I do frustum, occlusion(in future) culling, I can save 12 bytes on meshlet structure by culling via bounding sphere.
bool IsConeBackfacing(const vec3 cameraPosition, const vec3 coneAxis, const float coneCutoff, const vec3 center, const float radius)
{
    return dot(normalize(center - cameraPosition), coneAxis) >= coneCutoff + radius / length(center - cameraPosition);
}

vec3 DecodeConeAxis(int8_t coneAxis[3])
{
    return vec3(int32_t(coneAxis[0]) / 127.0, int32_t(coneAxis[1]) / 127.0, int32_t(coneAxis[2]) / 127.0);
}

float DecodeConeCutoff(int8_t coneCutoff)
{
    return int32_t(coneCutoff) / 127.0;
}

#endif

#define MAX_MESHLET_VERTEX_COUNT 64u  // AMD says for better utilization meshlet vertex count should be multiple of warp size.
#define MESHLET_LOCAL_GROUP_SIZE 32u  // AMD:64, NVidia:32
#define MESHLET_CONE_WEIGHT 0.5f

// The ‘6’ in 126 is not a typo. The first generation hardware allocates primitive indices in 128 byte granularity and and needs
// to reserve 4 bytes for the primitive count. Therefore 3 * 126 + 4 maximizes the fit into a 3 * 128 = 384 bytes block.
#define MAX_MESHLET_TRIANGLE_COUNT 124u

#ifdef __cplusplus

using vec2    = glm::vec2;
using u16vec2 = glm::u16vec2;
using u8vec4  = glm::u8vec4;
using vec3    = glm::vec3;
using vec4    = glm::vec4;
using mat4    = glm::mat4;
using i8vec3  = glm::i8vec3;

#endif

struct Meshlet
{
    /* offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data */
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    /* number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count */
    uint32_t vertexCount;
    uint32_t triangleCount;
    /* bounding sphere, useful for frustum and occlusion culling */
    vec3 center;
    float radius;

    /* 8-bit SNORM; decode using x/127.0 */
    int8_t coneAxis[3];
    int8_t coneCutoff; /* = cos(angle/2) */
};
