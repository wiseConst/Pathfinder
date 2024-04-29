#ifndef SCENE_H
#define SCENE_H

#include "Core/Core.h"
#include "Renderer/RendererCoreDefines.h"
#include <entt/entt.hpp>

namespace Pathfinder
{

class Scene final : private Uncopyable, private Unmovable
{
  public:
    Scene();
    ~Scene() override;

    AccelerationStructure CreateTLAS();

  private:
    entt::registry m_Registry = {};
};

}  // namespace Pathfinder

#endif