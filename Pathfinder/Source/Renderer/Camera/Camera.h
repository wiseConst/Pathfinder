#ifndef CAMERA_H
#define CAMERA_H

#include "Core/Core.h"
#include "Core/Math."

namespace Pathfinder
{

enum class ECameraType : uint8_t
{
    CAMERA_TYPE_NONE = 0,
    CAMERA_TPYE_ORTHOGRAPHIC,
    CAMERA_TYPE_PERSPECTIVE
};

class Camera : private Uncopyable, private Unmovable
{
  public:
    FORCEINLINE static Unique<Camera> Create() {}

  protected:
    glm::mat4 m_Transform  = glm::mat4(1.0f);
    glm::mat4 m_View       = glm::mat4(1.0f);
    glm::mat4 m_Projection = glm::mat4(1.0f);
};

}  // namespace Pathfinder

#endif  // CAMERA_H
