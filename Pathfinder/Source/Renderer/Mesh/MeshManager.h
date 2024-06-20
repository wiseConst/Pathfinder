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
    static void LoadSubmeshes(std::vector<Shared<Submesh>>& submeshes, const std::string& meshDir,
                              std::unordered_map<std::string, Shared<Texture>>& loadedTextures, const fastgltf::Asset& asset,
                              const size_t meshIndex);

    static Shared<Texture> LoadTexture(std::unordered_map<std::string, Shared<Texture>>& loadedTextures, const std::string& meshAssetsDir,
                                       const fastgltf::Asset& asset, const fastgltf::Material& materialAccessor, const size_t textureIndex,
                                       TextureSpecification& textureSpec, const bool bMetallicRoughness = false,
                                       const bool bFlipOnLoad = false);

    template <typename T>
    static void RemapVertexStream(const size_t remappedVertexCount, std::vector<T>& vertexStream,
                                  const std::vector<uint32_t>& remappedIndices)
    {
        std::vector<T> remmapedVertexStream(remappedVertexCount);
        meshopt_remapVertexBuffer(remmapedVertexStream.data(), vertexStream.data(), vertexStream.size(), sizeof(vertexStream[0]),
                                  remappedIndices.data());
        vertexStream = std::move(remmapedVertexStream);
    }

    MeshManager()  = delete;
    ~MeshManager() = default;
};

}  // namespace Pathfinder
