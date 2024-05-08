#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include "Core/Core.h"

namespace Pathfinder
{

class Scene;

class SceneManager
{
  public:
    static void Deserialize(Shared<Scene>& scene, const std::filesystem::path& sceneFilePath);
    static void Serialize(const Shared<Scene>& scene, const std::filesystem::path& sceneFilePath);

  private:
    SceneManager()     = delete;
    ~SceneManager() = default;
};

}  // namespace Pathfinder

#endif