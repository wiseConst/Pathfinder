#pragma once

#include <Core/Core.h>
#include <filesystem>

namespace Pathfinder
{

class Submesh;

class Mesh final : private Uncopyable, private Unmovable
{
public:
    explicit Mesh(const std::filesystem::path& meshPath);
    ~Mesh() { Destroy(); }

    NODISCARD static Shared<Mesh> Create(const std::string& meshPath);

    NODISCARD FORCEINLINE const auto& GetSubmeshes() const { return m_Submeshes; }

private:
    std::vector<Shared<Submesh>> m_Submeshes;

    void Destroy() { m_Submeshes.clear(); }
    Mesh() = delete;
};

} // namespace Pathfinder