#ifndef SCENESERIALIZER_H
#define SCENESERIALIZER_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"

namespace Pathfinder
{

class Scene;

class SceneSeriazlier
{
  public:
    static void Deserialize(Shared<Scene>& scene, const std::filesystem::path& sceneFilePath);
    static void Serialize(const Shared<Scene>& scene, const std::filesystem::path& sceneFilePath);

  private:
    SceneSeriazlier()  = delete;
    ~SceneSeriazlier() = default;
};

}  // namespace Pathfinder

#endif