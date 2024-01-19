#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"

#include <fastgltf/types.hpp>

namespace Pathfinder
{

class Buffer;
class Mesh final : private Uncopyable, private Unmovable
{
  public:
    Mesh() = delete;
    explicit Mesh(const std::string& meshPath);
    ~Mesh() override { Destroy(); }

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

    NODISCARD FORCEINLINE const auto& GetIndexBuffers() const { return m_IndexBuffers; }
    NODISCARD FORCEINLINE const auto& GetVertexPositionBuffers() const { return m_VertexPositionBuffers; }
    NODISCARD FORCEINLINE const auto& GetVertexAttributeBuffers() const { return m_VertexAttributeBuffers; }

    // TODO: Add materials
    NODISCARD FORCEINLINE const auto& IsOpaque() const { return m_bIsOpaque; }

  private:
    // TODO: Add submesh to hide this shit
    std::vector<Shared<Buffer>> m_VertexPositionBuffers;
    std::vector<Shared<Buffer>> m_VertexAttributeBuffers;
    std::vector<Shared<Buffer>> m_IndexBuffers;
    std::vector<Shared<Buffer>> m_Meshlets;

    std::vector<bool> m_bIsOpaque;

    void LoadImages(const auto& asset);
    void LoadSubmeshes(const fastgltf::Asset& asset, const fastgltf::Mesh& submesh);
    void Destroy();
};

}  // namespace Pathfinder

#endif  // MESH_H
