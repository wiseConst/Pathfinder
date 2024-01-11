#ifndef CAMERA_H
#define CAMERA_H

#include "Core/Core.h"
#include "Core/Math.h"

namespace Pathfinder
{

enum class ECameraType : uint8_t
{
    CAMERA_TPYE_ORTHOGRAPHIC = 0,
    CAMERA_TYPE_PERSPECTIVE
};

class Camera : private Uncopyable, private Unmovable
{
  public:
    virtual ~Camera() = default;

    NODISCARD static Shared<Camera> Create(const ECameraType cameraType);

  protected:
    glm::mat4 m_View       = glm::mat4(1.0f);
    glm::mat4 m_Projection = glm::mat4(1.0f);

    Camera() = default;
};

}  // namespace Pathfinder

#endif  // CAMERA_H
