#pragma once

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"
#include "Renderer/Texture.h"

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
    static AABB GenerateAABB(const std::vector<MeshPositionVertex>& points);

    static Sphere GenerateBoundingSphere(const std::vector<MeshPositionVertex>& points);
    static void OptimizeMesh(std::vector<uint32_t>& indices, std::vector<MeshPositionVertex>& rawVertices,
                             std::vector<MeshAttributeVertex>& attributeVertices);

    static void BuildMeshlets(const std::vector<uint32_t>& indices, const std::vector<MeshPositionVertex>& vertexPositions,
                              std::vector<Meshlet>& outMeshlets, std::vector<uint32_t>& outMeshletVertices,
                              std::vector<uint8_t>& outMeshletTriangles);

    static void LoadMesh(std::vector<Shared<Submesh>>& submeshes, const std::filesystem::path& meshFilePath);

    static SurfaceMesh GenerateUVSphere(const uint32_t sectorCount, const uint32_t stackCount);

  private:
    static void LoadSubmeshes(UnorderedMap<std::string, Shared<Texture>>& loadedTextures, std::vector<Shared<Submesh>>& submeshes,
                              const std::string& meshDir, const fastgltf::Asset& asset, const size_t meshIndex);

    MeshManager()  = delete;
    ~MeshManager() = default;
};

}  // namespace Pathfinder
