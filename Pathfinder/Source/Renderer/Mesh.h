#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"

#include <fastgltf/types.hpp>

namespace Pathfinder
{

class Buffer;

class Submesh final : private Uncopyable, private Unmovable
{
  public:
    Submesh() = default;
    ~Submesh() override { Destroy(); }

    NODISCARD FORCEINLINE const auto& GetIndexBuffer() const { return m_IndexBuffer; }
    NODISCARD FORCEINLINE const auto& GetVertexPositionBuffer() const { return m_VertexPositionBuffer; }
    NODISCARD FORCEINLINE const auto& GetVertexAttributeBuffer() const { return m_VertexAttributeBuffer; }
    NODISCARD FORCEINLINE const auto& GetMeshletBuffer() const { return m_MeshletBuffer; }

    NODISCARD FORCEINLINE const auto& IsOpaque() const { return m_bIsOpaque; }

  private:
    Shared<Buffer> m_VertexPositionBuffer;
    Shared<Buffer> m_VertexAttributeBuffer;
    Shared<Buffer> m_IndexBuffer;
    Shared<Buffer> m_MeshletBuffer;
    bool m_bIsOpaque = false;

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
