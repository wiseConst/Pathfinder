#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"

namespace Pathfinder
{

class Buffer;
class Mesh final : private Uncopyable, private Unmovable
{
  public:
    Mesh() = delete;
    explicit Mesh(const std::string& meshPath);
    ~Mesh() override;

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

  private:
    void Destroy();

    std::vector<Shared<Buffer>> m_VertexPositionBuffers;
    std::vector<Shared<Buffer>> m_VertexAttributeBuffers;

    void LoadImages(const auto& asset);
    void LoadSubmeshes(const auto& asset, const auto& submesh);
};

}  // namespace Pathfinder

#endif  // MESH_H
