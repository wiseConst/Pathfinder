
#ifdef __cplusplus
#pragma once

#else 
    // https://burtleburtle.net/bob/hash/integer.html
    uint32_t hash( uint32_t a)
    {
        a += ~(a<<15);
        a ^=  (a>>10);
        a +=  (a<<3);
        a ^=  (a>>6);
        a += ~(a<<11);
        a ^=  (a>>16);
        return a;
    }
    
#endif

#define MAX_MESHLET_VERTEX_COUNT 64u
#define MAX_MESHLET_TRIANGLE_COUNT 124u
#define MESHLET_LOCAL_GROUP_SIZE 32u // multiples of 32 - better
#define MESHLET_CONE_WEIGHT 0.5f

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
