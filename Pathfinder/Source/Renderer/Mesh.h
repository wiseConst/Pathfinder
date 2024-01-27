#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"

#include <fastgltf/types.hpp>

namespace Pathfinder
{

class Buffer;
class Material;

class Submesh final : private Uncopyable, private Unmovable
{
  public:
    Submesh() = default;
    ~Submesh() override { Destroy(); }

    NODISCARD FORCEINLINE const auto& GetIndexBuffer() const { return m_IndexBuffer; }
    NODISCARD FORCEINLINE const auto& GetVertexPositionBuffer() const { return m_VertexPositionBuffer; }
    NODISCARD FORCEINLINE const auto& GetVertexAttributeBuffer() const { return m_VertexAttributeBuffer; }

    NODISCARD FORCEINLINE const auto& GetMeshletBuffer() const { return m_MeshletBuffer; }
    NODISCARD FORCEINLINE const auto& GetMeshletVerticesBuffer() const { return m_MeshletVerticesBuffer; }
    NODISCARD FORCEINLINE const auto& GetMeshletTrianglesBuffer() const { return m_MeshletTrianglesBuffer; }

    NODISCARD FORCEINLINE auto& GetMaterial() const { return m_Material; }

    void SetMaterial(const Shared<Material> material) { m_Material = material; }

  private:
    Shared<Buffer> m_VertexPositionBuffer;
    Shared<Buffer> m_VertexAttributeBuffer;
    Shared<Buffer> m_IndexBuffer;
    Shared<Buffer> m_MeshletVerticesBuffer;
    Shared<Buffer> m_MeshletTrianglesBuffer;
    Shared<Buffer> m_MeshletBuffer;
    Shared<Material> m_Material;

    friend class Mesh;

    void Destroy();
};

class Mesh final : private Uncopyable, private Unmovable
{
  public:
    Mesh() = delete;
    explicit Mesh(const std::string& meshPath);
    ~Mesh() override { Destroy(); }

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

    NODISCARD FORCEINLINE const auto& GetSubmeshes() const { return m_Submeshes; }

  private:
    std::vector<Shared<Submesh>> m_Submeshes;

    void LoadImages(const auto& asset);
    void LoadSubmeshes(const fastgltf::Asset& asset, const fastgltf::Mesh& GLTFsubmesh);
    void Destroy();
};

}  // namespace Pathfinder

#endif  // MESH_H
