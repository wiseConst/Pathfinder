#pragma once

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"
#include "Renderer/Texture.h"

#include <meshoptimizer.h>

namespace fastgltf
{
class Asset;
class Material;
}  // namespace fastgltf

namespace Pathfinder
{

class Submesh;
class MeshManager final
{
  public:
    struct MeshoptimizeVertex
    {
        glm::vec3 Position = glm::vec3(0.0f);
        uint32_t Color     = 0xFFFFFFFF;
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

    static void LoadMesh(std::vector<Shared<Submesh>>& submeshes, const std::filesystem::path& meshFilePath);

    static SurfaceMesh GenerateUVSphere(const uint32_t sectorCount, const uint32_t stackCount);

  private:
    static void LoadSubmeshes(std::vector<Shared<Submesh>>& submeshes, const std::string& meshDir,
                              std::unordered_map<std::string, Shared<Texture>>& loadedTextures, const fastgltf::Asset& asset,
                              const size_t meshIndex);

    static Shared<Texture> LoadTexture(std::unordered_map<std::string, Shared<Texture>>& loadedTextures,
                                         const std::string& meshAssetsDir, const fastgltf::Asset& asset,
                                         const fastgltf::Material& materialAccessor, const size_t textureIndex,
                                         TextureSpecification& textureSpec, const bool bMetallicRoughness = false,
                                         const bool bFlipOnLoad = false);

    MeshManager()  = delete;
    ~MeshManager() = default;
};

}  // namespace Pathfinder
