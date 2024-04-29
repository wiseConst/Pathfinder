#ifndef MESHMANAGER_H
#define MESHMANAGER_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

#include <meshoptimizer.h>

namespace Pathfinder
{

class MeshManager final
{
  public:
    struct MeshoptimizeVertex
    {
        glm::vec3 Position = glm::vec3(0.0f);
        u8vec4 Color       = u8vec4(255);
        glm::vec3 Normal   = glm::vec3(0.0f);
        glm::vec3 Tangent  = glm::vec3(0.0f);
        glm::u16vec2 UV    = glm::u16vec2(0);
    };

    static AABB GenerateAABB(const std::vector<MeshPositionVertex>& points);

    static Sphere GenerateBoundingSphere(const std::vector<MeshPositionVertex>& points);
    static void OptimizeMesh(const std::vector<uint32_t>& srcIndices, const std::vector<MeshoptimizeVertex>& srcVertices,
                             std::vector<uint32_t>& outIndices, std::vector<MeshoptimizeVertex>& outVertices);

    static void BuildMeshlets(const std::vector<uint32_t>& indices, const std::vector<MeshPositionVertex>& vertexPositions,
                              std::vector<Meshlet>& outMeshlets, std::vector<uint32_t>& outMeshletVertices,
                              std::vector<uint8_t>& outMeshletTriangles);

  private:
    MeshManager()  = delete;
    ~MeshManager() = default;
};

}  // namespace Pathfinder

#endif