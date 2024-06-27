#include "PathfinderPCH.h"
#include "Mesh.h"

#include "Submesh.h"
#include "Core/Application.h"

#include "MeshManager.h"

namespace Pathfinder
{

Mesh::Mesh(const std::filesystem::path& meshPath)
{
    MeshManager::LoadMesh(m_Submeshes, meshPath);
}

Shared<Mesh> Mesh::Create(const std::string& meshPath)
{
    PFR_ASSERT(!meshPath.empty(), "Mesh path is empty!");
    const auto& appSpec           = Application::Get().GetSpecification();
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    const std::filesystem::path fullMeshPath = workingDirFilePath / appSpec.AssetsDir / appSpec.MeshDir / meshPath;
    std::string fullMeshPathString           = fullMeshPath.string();
    std::replace(fullMeshPathString.begin(), fullMeshPathString.end(), '\\', '/');  // adjust
    return MakeShared<Mesh>(fullMeshPathString);
}

}  // namespace Pathfinder