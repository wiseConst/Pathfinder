#ifndef SUBMESH_H
#define SUBMESH_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"
#include "Globals.h"

namespace Pathfinder
{

class Buffer;
class Material;
class Texture2D;

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
    NODISCARD FORCEINLINE const auto& GetBoundingSphere() const { return m_BoundingSphere; }

    void SetMaterial(const Shared<Material>& material) { m_Material = material; }

  private:
    Shared<Buffer> m_VertexPositionBuffer;
    Shared<Buffer> m_VertexAttributeBuffer;
    Shared<Buffer> m_IndexBuffer;
    Shared<Buffer> m_MeshletVerticesBuffer;
    Shared<Buffer> m_MeshletTrianglesBuffer;
    Shared<Buffer> m_MeshletBuffer;
    Shared<Material> m_Material;

    Sphere m_BoundingSphere = {};

    friend class Mesh;

    void Destroy();
};

}  // namespace Pathfinder

#endif