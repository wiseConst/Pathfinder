
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

#define MAX_MESHLET_TRIANGLE_COUNT 124
#define MAX_MESHLET_VERTEX_COUNT 64

struct Meshlet
{
    uint32_t vertices[MAX_MESHLET_VERTEX_COUNT];
    uint8_t triangles[MAX_MESHLET_TRIANGLE_COUNT*3];
    uint8_t vertexCount;
    uint8_t triangleCount;
};