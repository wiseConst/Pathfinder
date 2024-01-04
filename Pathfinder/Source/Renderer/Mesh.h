#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"

namespace Pathfinder
{

class Mesh final : private Uncopyable, private Unmovable
{
  public:
    Mesh() = delete;
    explicit Mesh(const std::string& meshPath);
    ~Mesh() override;

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

  private:
    void Destroy();
};

}  // namespace Pathfinder

#endif  // MESH_H
