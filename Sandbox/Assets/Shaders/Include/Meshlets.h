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

bool ConeCull(const vec3 cameraPosition, const vec3 coneApex, const vec3 coneAxis, const float coneCutoff)
{
    return dot(normalize(coneApex - cameraPosition), normalize(coneAxis)) >= coneCutoff;
}

#endif

#define MAX_MESHLET_VERTEX_COUNT 64u  // AMD says for better utilization meshlet vertex count should be multiple of warp size.
#define MESHLET_LOCAL_GROUP_SIZE 32u  // AMD:64, NVidia:32
#define MESHLET_CONE_WEIGHT 0.5f

// The ‘6’ in 126 is not a typo. The first generation hardware allocates primitive indices in 128 byte granularity and and needs
// to reserve 4 bytes for the primitive count. Therefore 3 * 126 + 4 maximizes the fit into a 3 * 128 = 384 bytes block.
#define MAX_MESHLET_TRIANGLE_COUNT 124u

struct Meshlet
{
    /* offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data */
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    /* number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count */
    uint32_t vertexCount;
    uint32_t triangleCount;
    /* bounding sphere, useful for frustum and occlusion culling */
    float center[3];
    float radius;
    /* normal cone, useful for backface culling */
    float coneApex[3];
    float coneAxis[3];
    float coneCutoff; /* = cos(angle/2) */
};

