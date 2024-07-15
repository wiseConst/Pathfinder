// NVidia advises to stay this struct below 108 bytes, or at least under 236 bytes. https://developer.nvidia.com/blog/advanced-api-performance-mesh-shaders/
struct MeshletTaskData
{
    uint32_t baseMeshletID; // where the meshletIDs started from for this task workgroup
    uint8_t meshlets[MESHLET_LOCAL_GROUP_SIZE];  // Stores meshlet IDs
};

taskPayloadSharedEXT MeshletTaskData tp_TaskData;