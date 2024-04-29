#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

namespace fastgltf
{
class Asset;
}

namespace Pathfinder
{

class Submesh;
class Texture2D;
class Mesh final : private Uncopyable, private Unmovable
{
  public:
    explicit Mesh(const std::filesystem::path& meshPath);
    ~Mesh() override { Destroy(); }

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

    NODISCARD FORCEINLINE const auto& GetSubmeshes() const { return m_Submeshes; }

  private:
    std::vector<Shared<Submesh>> m_Submeshes;

    void LoadSubmeshes(const std::string& meshDir, std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures,
                       const fastgltf::Asset& asset, const size_t meshIndex);
    void Destroy() { m_Submeshes.clear(); }

    Mesh() = delete;
};

}  // namespace Pathfinder

#endif  // MESH_H
