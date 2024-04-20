#ifndef MESH_H
#define MESH_H

#include "Core/Core.h"
#include "Submesh.h"
#include "Renderer/RendererCoreDefines.h"
#include "Globals.h"

namespace fastgltf
{
class Asset;
}

namespace Pathfinder
{

class Mesh final : private Uncopyable, private Unmovable
{
  public:
    Mesh() = delete;
    explicit Mesh(const std::filesystem::path& meshPath);
    ~Mesh() override { Destroy(); }

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

    NODISCARD FORCEINLINE const auto& GetSubmeshes() const { return m_Submeshes; }

#if TODO
    NODISCARD FORCEINLINE const auto& GetTLAS() const { return m_TLAS; }
#endif

  private:
    std::vector<Shared<Submesh>> m_Submeshes;

#if TODO
    AccelerationStructure m_TLAS = {};
    std::vector<AccelerationStructure> m_BLASes;
#endif

    void LoadSubmeshes(const std::string& meshDir, std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures,
                       const fastgltf::Asset& asset, const size_t meshIndex);
    void Destroy();
};

}  // namespace Pathfinder

#endif  // MESH_H
